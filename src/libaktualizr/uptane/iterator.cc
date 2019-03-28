#include "iterator.h"

namespace Uptane {

Targets getTrustedDelegation(const Role &delegate_role, const Targets &parent_targets,
                             const ImagesRepository &images_repo, INvStorage &storage, Fetcher &fetcher) {
  std::string delegation_meta;

  auto version_in_snapshot = images_repo.getRoleVersion(delegate_role);

  if (storage.loadDelegation(&delegation_meta, delegate_role)) {
    auto version = extractVersionUntrusted(delegation_meta);

    if (version > version_in_snapshot) {
      throw SecurityException("images", "Rollback attempt on delegated targets");
    } else if (version < version_in_snapshot) {
      delegation_meta.clear();
      storage.deleteDelegation(delegate_role);
    }
  }

  bool delegation_remote = delegation_meta.empty();
  if (delegation_remote) {
    if (!fetcher.fetchLatestRole(&delegation_meta, Uptane::kMaxImagesTargetsSize, RepositoryType::Image(),
                                 delegate_role)) {
      throw Uptane::DelegationMissing(delegate_role.ToString());
    }
  }

  if (!images_repo.verifyRoleHashes(delegation_meta, delegate_role)) {
    throw Uptane::DelegationHashMismatch(delegate_role.ToString());
  }

  auto delegation = ImagesRepository::verifyDelegation(delegation_meta, delegate_role, parent_targets);

  if (delegation == nullptr) {
    throw SecurityException("images", "Delegation verification failed");
  }

  if (delegation_remote) {
    if (delegation->version() != version_in_snapshot) {
      throw VersionMismatch("images", delegate_role.ToString());
    }
    storage.storeDelegation(delegation_meta, delegate_role);
  }

  return *delegation;
}

LazyTargetsList::DelegationIterator::DelegationIterator(const ImagesRepository &repo,
                                                        std::shared_ptr<INvStorage> storage,
                                                        std::shared_ptr<Fetcher> fetcher, bool is_end)
    : repo_{repo}, storage_{std::move(storage)}, fetcher_{std::move(fetcher)}, is_end_{is_end} {
  tree_ = std::make_shared<DelegatedTargetTreeNode>();
  tree_node_ = tree_.get();

  tree_node_->role = Role::Targets();
  cur_targets_ = repo_.getTargets();
}

void LazyTargetsList::DelegationIterator::renewTargetsData() {
  auto role = tree_node_->role;

  if (role == Role::Targets()) {
    cur_targets_ = repo_.getTargets();
  } else {
    // go to the top of the delegation tree
    std::stack<std::vector<std::shared_ptr<DelegatedTargetTreeNode>>::size_type> indices;
    auto node = tree_node_->parent;
    while (node->parent != nullptr) {
      indices.push(node->parent_idx);
      node = node->parent;
    }

    auto parent_targets = repo_.getTargets();
    while (!indices.empty()) {
      auto idx = indices.top();
      indices.pop();

      auto fetched_role = Role(parent_targets->delegated_role_names_[idx], true);
      parent_targets = std::make_shared<const Targets>(
          getTrustedDelegation(fetched_role, *parent_targets, repo_, *storage_, *fetcher_));
    }
    cur_targets_ = std::make_shared<Targets>(getTrustedDelegation(role, *parent_targets, repo_, *storage_, *fetcher_));
  }
}

bool LazyTargetsList::DelegationIterator::operator==(const LazyTargetsList::DelegationIterator &other) const {
  if (is_end_ && other.is_end_) {
    return true;
  }

  return is_end_ == other.is_end_ && tree_node_->role == other.tree_node_->role && target_idx_ == other.target_idx_;
}

const Target &LazyTargetsList::DelegationIterator::operator*() {
  if (is_end_) {
    throw std::runtime_error("Inconsistent delegation iterator");  // TODO: UptaneException
  }

  if (!cur_targets_) {
    renewTargetsData();
  }

  if (target_idx_ >= cur_targets_->targets.size()) {
    throw std::runtime_error("Inconsistent delegation iterator");  // TODO: UptaneException
  }

  return cur_targets_->targets[target_idx_];
}

LazyTargetsList::DelegationIterator LazyTargetsList::DelegationIterator::operator++() {
  if (is_end_) {
    return *this;
  }

  if (!cur_targets_) {
    renewTargetsData();
  }

  // first iterate over current role's targets
  if (target_idx_ + 1 < cur_targets_->targets.size()) {
    ++target_idx_;
    return *this;
  }

  // then go to children delegations
  if (terminating_ || level_ >= kDelegationsMaxDepth) {
    // but only if this delegation is not terminating
    is_end_ = true;
    return *this;
  }

  // populate the next level of the delegation tree if it's not populated yet
  if (cur_targets_->delegated_role_names_.size() != tree_node_->children.size()) {
    tree_node_->children.clear();

    for (std::vector<std::shared_ptr<DelegatedTargetTreeNode>>::size_type i = 0;
         i < cur_targets_->delegated_role_names_.size(); ++i) {
      auto new_node = std::make_shared<DelegatedTargetTreeNode>();

      new_node->role = Role(cur_targets_->delegated_role_names_[i], true);
      new_node->parent = tree_node_;
      new_node->parent_idx = i;

      tree_node_->children.push_back(new_node);
    }
  }

  if (children_idx_ < tree_node_->children.size()) {
    auto new_tree_node = tree_node_->children[children_idx_].get();
    target_idx_ = 0;
    children_idx_ = 0;
    ++level_;

    auto terminating_it = cur_targets_->terminating_role_.find(new_tree_node->role);
    if (terminating_it == cur_targets_->terminating_role_.end()) {
      throw std::runtime_error("Inconsistent delegation tree");
    }
    terminating_ = terminating_it->second;

    cur_targets_.reset();
    tree_node_ = new_tree_node;
    return *this;
  }

  // then go to the parent delegation
  if (tree_node_->parent != nullptr) {
    auto new_tree_node = tree_node_->parent;
    children_idx_ = tree_node_->parent_idx + 1;
    --level_;
    terminating_ = false;

    cur_targets_.reset();
    tree_node_ = new_tree_node;
    renewTargetsData();
    target_idx_ = cur_targets_->targets.size();  // mark targets as exhausted
    return ++(*this);                            // reiterate to find the next target
  }

  // then give up
  is_end_ = true;
  return *this;
}

}  // namespace Uptane
