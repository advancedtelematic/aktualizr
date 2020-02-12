Contributing to the aktualizr project
=====

We welcome code contributions from the community and are very happy to receive feedback, issue reports and code in the form of pull requests.

Issue Tracker
----

This project uses the Github issue tracker. Please report bugs, issues and feature requests there or email us at otaconnect.support@here.com.

Code quality and style
----

All code should be developed according to the [Google C++ style guide](https://google.github.io/styleguide/cppguide.html). In addition, the code should conform to the following guidelines:

* All code should be covered by tests.
   - Ideally, all tests should be automated and run in CI.
   - If automated testing is not possible, manual test cases should be described.
   - Tests cases should at least exercise all documented requirements.
* It must be easy for a new developer to run the test suite based on the information in the [readme](README.adoc).
* All code must pass formatting and static linting tests.
* Only working code that passes all tests in CI will be merged into the master branch.
   - The master branch should always be in a deployable state.
   - Functionally incomplete code is not necessarily undeployable.
* Pull requests should only contain a single feature, fix, or refactoring.
   - Developers should not create large, month-long, multiple-feature branches.
     - The larger the PR, the harder it is to review, the more likely it will need to be rebased, and the harder it is to rebase it.
     - If a PR changes thousands of lines of code, please consider splitting it into smaller PRs.
   - Developers should aim to have their code merged at least once every week.
   - Multiple small fixes or refactorings can be grouped together in one PR, but each independent fix or refactoring should be a unique commit.
* Make separate commits for logically separate changes within a PR.
   - Ideally, each commit should be buildable and should pass the tests (to simplify git bisect).
   - The short description (first line of the commit text) should not exceed 72 chars. The rest of the text (if applicable) should be separated by an empty line.
* All code must be reviewed before it is merged to the master branch.
   - The reviewer can not be the code author.
   - All the reviewer's concerns must be resolved to reviewer's satisfaction.
   - Reviews should enforce the coding guidelines for the project.
* Bugs should be reproduced with a failing test case before they are resolved.
   - Reviewers of code including bug fixes should check that a corresponding test has been included with the fix.
* Code that is unfinished or that requires further review should be indicated as such with a `TODO` comment.
   - If appropriate, this comment should include a reference to a Jira ticket that describes the work to be done in detail.
   - Since external contributors do not have access to our Jira, the comment must at least briefly describe the work to be done.
* New features, bug fixes, and removed functionality should be documented in the [changelog](CHANGELOG.md).

Making a Pull Request
----

When you start developing a feature, please create a feature branch that includes the type of branch, the ID of the github issue or Jira ticket if available, and a brief description. For example `feat/9/https-support`, `fix/OTA-123/fix-token-expiry` or `refactor/tidy-up-imports`. Please do not mix feature development, bugfixes and refactoring into the same branch.

When your feature is ready, push the branch and make a pull request. We will review the request and give you feedback. Once the code passes the review it can be merged into master and the branch can be deleted.

Continuous Integration (CI)
----

We currently have two CI servers: Travis CI and gitlab. Travis CI is usually slower and flakier, and we don't run the tests that require provisioning credentials on it, but it is publicly accessible. Gitlab is more powerful and is the source of truth, but it is inaccessible to external contributors. Normally, we expect PRs to pass CI on both CI servers, but if Travis CI is particularly unreliable, we sometimes make exceptions and ignore it.

PRs from external contributors will not automatically trigger CI on gitlab. If the PR is small and we believe that passing Travis CI is good enough, we will merge it if that succeeds. Otherwise, we can trigger gitlab to run CI on your PR by manually pushing the branch to gitlab.

PRs that only affect documentation do not strictly need to pass CI. As such, gitlab does not run CI on branches that start with "docs/". Please do not make code changes in a branch with that prefix.

Developer Certificate of Origin (DCO)
----

All commits in pull requests must contain a `Signed-off-by:` line to indicate that the developer has agreed to the terms of the [Developer Certificate of Origin](https://developercertificate.org):

~~~~
Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
~~~~

A simple way to sign off is to use the `-s` flag of `git commit`.

New pull requests will automatically be checked by the [probot/dco](https://probot.github.io/apps/dco/).
