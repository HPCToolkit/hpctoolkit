// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2019-2020, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#include "context.hpp"

#include "module.hpp"
#include "metric.hpp"

#include <stack>
#include <stdexcept>
#include <vector>
#include <cassert>

using namespace hpctoolkit;

Context::Context(ud_t::struct_t& rs, Context* p, Scope s)
  : userdata(rs, std::ref(*this)), children_p(new children_t()),
    u_parent(p), u_scope(s) {};
Context::Context(Context&& c)
  : userdata(std::move(c.userdata), std::ref(*this)),
    children_p(new children_t()),
    u_parent(c.direct_parent()), u_scope(c.scope()) {};

Context::~Context() noexcept {
  // C++ generates a recursive algorithm for this by default
  // So we replace it with a post-order tree traversal
  try {
    iterate(nullptr, [](Context& c) { c.children_p.reset(nullptr); });
  } catch(...) {};  // If we run into errors, just let the recursion handle it.
}

void Context::iterate(const std::function<void(Context&)>& pre,
                      const std::function<void(Context&)>& post) {
  struct frame_t {
    frame_t(Context& c)
      : ctx(c), iter(c.children_p->iterate()),
        here(iter.begin()), end(iter.end()) {};
    Context& ctx;
    using iter_t = decltype(children_p->iterate());
    iter_t iter;
    decltype(iter.begin()) here;
    decltype(iter.end()) end;
  };
  std::stack<frame_t, std::vector<frame_t>> stack;
  if(pre) pre(*this);
  if(children_p) stack.emplace(*this);
  while(!stack.empty()) {
    if(stack.top().here != stack.top().end) {
      // We have more children to handle
      Context& c = *stack.top().here++;
      if(pre) pre(c);
      if(c.children_p) stack.emplace(c);
      continue;
    }

    Context& c = stack.top().ctx;
    stack.pop();
    if(post) post(c);
  }
}

void Context::citerate(const std::function<void(const Context&)>& pre,
                       const std::function<void(const Context&)>& post) const {
  struct frame_t {
    frame_t(const Context& c)
      : ctx(c), iter(c.children_p->citerate()),
        here(iter.begin()), end(iter.end()) {};
    const Context& ctx;
    using iter_t = decltype(children_p->citerate());
    iter_t iter;
    decltype(iter.begin()) here;
    decltype(iter.end()) end;
  };
  std::stack<frame_t, std::vector<frame_t>> stack;
  if(pre) pre(*this);
  if(children_p) stack.emplace(*this);
  while(!stack.empty()) {
    if(stack.top().here != stack.top().end) {
      // We have more children to handle
      const Context& c = *stack.top().here++;
      if(pre) pre(c);
      if(c.children_p) stack.emplace(c);
      continue;
    }

    const Context& c = stack.top().ctx;
    stack.pop();
    if(post) post(c);
  }
}

std::pair<Context&,bool> Context::ensure(Scope s) {
  auto x = children_p->emplace(userdata.base(), this, std::move(s));
  return {x.first(), x.second};
}

SuperpositionedContext& Context::superposition(std::vector<SuperpositionedContext::Target> targets) {
  for(const auto& targ: targets) {
    for(ContextRef t: targ.route) {
      if(auto tc = std::get_if<Context>(t)) {
        Context* c = &*tc;
        for(; c != nullptr && c != this; c = c->direct_parent());
        if(c == nullptr)
          util::log::fatal{} << "Attempt to route via a non-decendant Context in a Superposition: "
            << tc->scope() << " is not a child of " << scope();
      } else if(auto tc = std::get_if<SuperpositionedContext>(t)) {
        if(&tc->m_root != this)
          util::log::fatal{} << "Attempt to route via an incorrectly rooted Superposition in a Superposition: "
            << tc->m_root.scope() << " is not " << scope();
      } else
        util::log::fatal{} << "Attempt to route via an unsupported Context in a Superposition!";
    }
    if(!std::holds_alternative<Context>(targ.target))
      util::log::fatal{} << "Attempt to target an improper Context in a Superposition!";
  }

  auto c = new SuperpositionedContext(*this, std::move(targets));
  superpositionRoots.emplace(c);
  return *c;
}

static util::optional_ref<CollaborativeContext> collaborationFor(const ContextRef& v) {
  if(auto pcc = std::get_if<Context, const CollaboratorRoot>(v))
    return pcc->second->second->collaboration();
  if(auto pc = std::get_if<CollaborativeSharedContext>(v))
    return pc->collaboration();
  return std::nullopt;
}

SuperpositionedContext::SuperpositionedContext(Context& root, std::vector<Target> targets)
  : m_root(root), m_targets(std::move(targets)) {
  if(m_targets.size() < 2)
    util::log::fatal{} << "Attempt to create a Superposition without enough Targets!";
  if(std::any_of(++m_targets.begin(), m_targets.end(), [&](const auto& t){
    return collaborationFor(t.target) != collaborationFor(m_targets.front().target);
  }))
    util::log::fatal{} << "Attempt to create a Superposition with inconsistent collaborations between Targets!";
}

SuperpositionedContext::Target::Target(std::vector<ContextRef> r, ContextRef t)
  : route(std::move(r)), target(t) {
  if(std::holds_alternative<CollaborativeContext>(t))
    util::log::fatal{} << "Attempt to create a Superposition target targeting a collaborative root!";
  if(std::any_of(route.begin(), route.end(), [&](const auto& r) {
    return std::holds_alternative<CollaborativeContext>(r);
  }))
    util::log::fatal{} << "Attempt to create a Superposition target with a collaborative root!";
  if(std::any_of(route.begin(), route.end(), [&](const auto& r) {
    return collaborationFor(r) != collaborationFor(target);
  }))
    util::log::fatal{} << "Attempt to create a Superposition target with inconsistent collaboration!";
}

CollaborativeSharedContext& CollaborativeSharedContext::ensure(Scope s,
    const std::function<void(Context&)>& onadd) noexcept {
  std::unique_lock<std::mutex> l(m_root.m_lock);
  return ensure(s, onadd, l);
}

CollaborativeSharedContext&
CollaborativeSharedContext::ensure(Scope s, const std::function<void(Context&)>& onadd,
                                   std::unique_lock<std::mutex>& l) noexcept {
  {
    auto it = m_children.find(s);
    if(it != m_children.end())
      return *it->second;
  }
  auto& child = *m_children.emplace(s, new CollaborativeSharedContext(m_root, this)).first->second;
  for(auto& [root, real]: m_shadowing) {
    assert(!m_dummy || real.direct_parent() == &root.get());
    auto x = m_dummy ? root->ensure(s) : real.ensure(s);
    if(x.second) onadd(x.first);
    child.m_shadowing.emplace(root, x.first);
  }
  return child;
}

void CollaborativeSharedContext::makeDummy() {
  if(m_parent != nullptr)
    util::log::fatal{} << "Only top-level SharedContexts can be dummies!";
  std::unique_lock<std::mutex> l(m_root.m_lock);
  if(m_dummy) return;
  if(!m_children.empty())
    util::log::fatal{} << "Attempt to make a SharedContext a dummy after children had been added!";
  m_dummy = true;
}

const CollaboratorRoot&
CollaborativeContext::ensure(Scope s, const std::function<void(Context&)>& onadd) noexcept {
  std::unique_lock<std::mutex> l(m_lock);
  {
    auto it = m_shadow.find({s, nullptr});
    if(it != m_shadow.end())
      return *it;
  }
  const auto& sroot = *m_shadow.emplace(s, new CollaborativeSharedContext(*this, nullptr)).first;
  auto& child = *sroot.second;
  for(Context& root: m_roots) {
    auto x = root.ensure(s);
    if(x.second) onadd(x.first);
    child.m_shadowing.emplace(root, x.first);
  }
  return sroot;
}

void CollaborativeContext::addCollaboratorRoot(ContextRef rootref,
    const std::function<void(Context&)>& onadd) noexcept {
  if(!std::holds_alternative<Context>(rootref))
    util::log::fatal{} << "Collaborator roots must be proper Contexts!";
  std::unique_lock<std::mutex> l(m_lock);
  Context& root = std::get<Context>(rootref);
  if(!m_roots.emplace(root).second)
    return;  // Its already been added, don't continue

  // Make a copy of the shadow-tree rooted at root
  using frame_t = std::pair<std::reference_wrapper<CollaborativeSharedContext>,
                            std::reference_wrapper<Context>>;
  std::stack<frame_t, std::vector<frame_t>> q;
  for(const auto& sc: m_shadow) {
    if(sc.second->m_dummy) {
      q.emplace(*sc.second, root);
    } else {
      auto x = root.ensure(sc.first);
      if(x.second) onadd(x.first);
      sc.second->m_shadowing.emplace(root, x.first);
      q.emplace(*sc.second, x.first);
    }
    while(!q.empty()) {
      auto& shad = q.top().first.get();
      auto& par = q.top().second.get();
      q.pop();

      for(const auto& sc: shad.m_children) {
        auto x = par.ensure(sc.first);
        if(x.second) onadd(x.first);
        sc.second->m_shadowing.emplace(root, x.first);
        q.emplace(*sc.second, x.first);
      }
    }
  }
}
