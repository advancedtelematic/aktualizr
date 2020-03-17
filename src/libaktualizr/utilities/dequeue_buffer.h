#ifndef UPTANE_DEQUEUE_BUFFER_H_
#define UPTANE_DEQUEUE_BUFFER_H_

#include <array>
#include <cstddef>

/**
 * A dequeue based on a contiguous buffer in memory. Used for buffering
 * data between recv() and ber_decode()
 */
class DequeueBuffer {
 public:
  /**
   * A pointer to the first element that has not been Consumed().
   */
  char *Head();

  /**
   * The number of elements that are valid (have been written) after Head()
   */
  size_t Size() const;

  /**
   * Called after bytes have been read from Head(). Remove them from the head
   * of the queue.
   */
  void Consume(size_t bytes);

  /**
   * A pointer to the next place to write data to
   */
  char *Tail();

  /**
   * The number of bytes beyond Tail() that are allocated and may be written to.
   */
  size_t TailSpace();

  /**
   * Call to indicate that bytes have been written in the range
   * Tail() ... Tail() + TailSpace().
   */
  void HaveEnqueued(size_t bytes);

 private:
  static constexpr int kSentinel = 1337161494;

  /**
   * buffer_[0..written_bytes_] contains to contents of this dequeue
   */
  size_t written_bytes_{0};
  std::array<char, 4096> buffer_{};  // Zero initialise as a security pesimisation
  // NOLINTNEXTLINE (should be just for clang-diagnostic-unused-private-field but it won't work)
  int sentinel_{kSentinel};  // Sentinel to check for writers overflowing buffer_
};

#endif  // UPTANE_DEQUEUE_BUFFER_H_
