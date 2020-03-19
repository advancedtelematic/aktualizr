#include "utilities/dequeue_buffer.h"

#include <cassert>
#include <cstring>
#include <stdexcept>
char* DequeueBuffer::Head() {
  assert(sentinel_ == kSentinel);
  return buffer_.data();
}

size_t DequeueBuffer::Size() const {
  assert(sentinel_ == kSentinel);
  return written_bytes_;
}

void DequeueBuffer::Consume(size_t bytes) {
  assert(sentinel_ == kSentinel);
  // It would be possible to have a smarter algorithm here that
  // only shuffles bytes down when Tail() is called and we are getting
  // close to the end of the buffer, or when the buffer is nearly empty
  // the memmove operation is cheap. This isn't performance critical code.
  if (written_bytes_ < bytes) {
    throw std::logic_error("Attempt to DequeueBuffer::Consume() more bytes than are valid");
  }
  // Shuffle up the buffer
  auto* next_unconsumed_byte = buffer_.begin() + bytes;
  auto* end_of_written_area = buffer_.begin() + written_bytes_;
  std::copy(next_unconsumed_byte, end_of_written_area, buffer_.begin());
  written_bytes_ -= bytes;
}

char* DequeueBuffer::Tail() {
  assert(sentinel_ == kSentinel);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return buffer_.data() + written_bytes_;
}

size_t DequeueBuffer::TailSpace() {
  assert(sentinel_ == kSentinel);
  return buffer_.size() - written_bytes_;
}

void DequeueBuffer::HaveEnqueued(size_t bytes) {
  // Assert first. If we have corrupted memory, there is
  // no amount of exception handling that will save us. Abort rather
  // than risk a security hole
  assert(sentinel_ == kSentinel);
  if (buffer_.size() < written_bytes_ + bytes) {
    throw std::logic_error("Wrote bytes beyond the end of the buffer");
  }
  written_bytes_ += bytes;
}
