#ifndef SP_MAINLINE_DHT_STACK_H
#define SP_MAINLINE_DHT_STACK_H

#include <cstddef>
#include <utility>

namespace sp {
//=====================================
template <typename T>
struct dstack_node {
  dstack_node<T> *next;
  T value;

  template <typename... Arg>
  dstack_node(Arg &&...) noexcept;

  ~dstack_node();
};

//=====================================
template <typename T>
struct dstack {
  dstack_node<T> *root;
  std::size_t length;

  dstack() noexcept;
  ~dstack();
};

//=====================================
template <typename T>
std::size_t
length(const dstack<T> &) noexcept;

//=====================================
template <typename T>
bool
push(dstack<T> &, dstack_node<T> *) noexcept;

template <typename T, typename... Arg>
bool
push(dstack<T> &, Arg &&...) noexcept;

//=====================================
template <typename T>
bool
pop(dstack<T> &, dstack_node<T> *&) noexcept;

//=====================================
template <typename T>
void
reclaim(dstack<T> &, dstack_node<T> *) noexcept;

//=====================================
template <typename T, typename F>
void
for_each(const dstack<T> &, F) noexcept;

//=====================================
template <typename T, typename F>
bool
for_all(const dstack<T> &, F) noexcept;

//=====================================
//====Implementation===================
//=====================================
template <typename T>
template <typename... Arg>
dstack_node<T>::dstack_node(Arg &&... args) noexcept
    : next{nullptr}
    , value(std::forward<Arg...>(args)...) {
}

template <typename T>
dstack_node<T>::~dstack_node() {
}

//=====================================
template <typename T>
dstack<T>::dstack() noexcept
    : root{nullptr}
    , length{0} {
}

template <typename T>
dstack<T>::~dstack() {
}

//=====================================
template <typename T>
std::size_t
length(const dstack<T> &self) noexcept {
  return self.length;
}

//=====================================
template <typename T>
bool
push(dstack<T> &self, dstack_node<T> *in) noexcept {
  assertx(in);
  assertx(!in->next);

  if (in) {
    in->next = self.root;
    self.root = in;
    ++self.length;

    return true;
  }
  return false;
}

template <typename T, typename... Arg>
bool
push(dstack<T> &self, Arg &&... args) noexcept {
  auto in = new sp::dstack_node<T>(std::forward<Arg...>(args)...);
  if (in) {
    return push(self, in);
  }

  return false;
}

//=====================================
template <typename T>
bool
pop(dstack<T> &self, dstack_node<T> *&result) noexcept {
  assertx(!result);
  result = self.root;

  if (result) {
    self.root = result->next;
    result->next = nullptr;
    --self.length;
  }

  return result;
}

//=====================================
template <typename T>
void
reclaim(dstack<T> &, dstack_node<T> *in) noexcept{
  if(in){
    delete in;
  }
}

//=====================================
template <typename T, typename F>
void
for_each(const dstack<T> &self, F f) noexcept {
  auto it = self.root;
  while (it) {
    const T &value = it->value;
    f(value);
    it = it->next;
  }
}

//=====================================
template <typename T, typename F>
bool
for_all(const dstack<T> &self, F f) noexcept {
  auto it = self.root;
  while (it) {
    const T &value = it->value;
    if (!f(value)) {
      return false;
    }
    it = it->next;
  }
  return true;
}

//=====================================
} // namespace sp

#endif
