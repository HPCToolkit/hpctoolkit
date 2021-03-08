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

SuperpositionedContext::SuperpositionedContext(Context& root, std::vector<Target> targets)
  : m_root(root), m_targets(std::move(targets)) {
  if(m_targets.size() < 2)
    util::log::fatal{} << "Attempt to create a Superposition without enough Targets!";
  if(std::any_of(++m_targets.begin(), m_targets.end(), [&](const auto& t){
    return t.target.collaboration() != m_targets.front().target.collaboration();
  }))
    util::log::fatal{} << "Attempt to create a Superposition with inconsistent collaborations between Targets!";
}

SuperpositionedContext::Target::Target(std::vector<ContextRef> r, ContextRef t)
  : route(std::move(r)), target(t) {
  if(std::any_of(route.begin(), route.end(), [&](const auto& r) {
    return r.collaboration() != target.collaboration();
  }))
    util::log::fatal{} << "Attempt to create a Superposition target with inconsistent collaboration!";
}

Context& CollaborativeContext::ensure(Context& c, Scope s, const std::function<void(Context&)>& onadd) noexcept {
  std::unique_lock<std::mutex> l(m_lock);
  auto it = m_shadowMap.find(c);
  if(it == m_shadowMap.end())
    util::log::fatal{} << "Invalid attempt to expand a Collaborative subtree!";
  auto& [root, shad] = it->second;
  return shad.ensure(*this, s, onadd).m_shadowing.at(root);
}

CollaborativeContext::ShadowContext&
CollaborativeContext::ShadowContext::ensure(CollaborativeContext& collab, Scope s,
                                            const std::function<void(Context&)>& onadd) noexcept {
  {
    auto it = m_children.find(s);
    if(it != m_children.end())
      return *it->second;
  }
  auto& child = *m_children.emplace(s, std::make_unique<ShadowContext>()).first->second;
  for(auto& [root, real]: m_shadowing)
    child.update(collab, root, real, s, onadd);
  return child;
}

void CollaborativeContext::ShadowContext::update(CollaborativeContext& collab,
    Context& root, Context& par, Scope s, const std::function<void(Context&)>& onadd) noexcept {
  auto x = par.ensure(s);
  if(x.second) onadd(x.first);
  m_shadowing.emplace(root, x.first);
  if(!m_unique) m_unique = x.first;
  collab.m_shadowMap.insert({x.first, {root, *this}});
}

void CollaborativeContext::addCollaboratorRoot(ContextRef root, const std::function<void(Context&)>& onadd) noexcept {
  if(root.collaboration())
    util::log::fatal{} << "Collaborator roots must not be part of collaborations already!";
  if(!std::holds_alternative<Context>(root))
    util::log::fatal{} << "Collaborator roots must be proper Contexts!";
  std::unique_lock<std::mutex> l(m_lock);
  Context& rroot = std::get<Context>(root);
  if(!m_shadow.m_shadowing.emplace(rroot, rroot).second) return;
  if(!m_shadow.m_unique) m_shadow.m_unique = rroot;
  m_shadowMap.insert({rroot, {rroot, m_shadow}});

  // Make a copy of the shadow-tree rooted at root
  std::stack<std::reference_wrapper<ShadowContext>,
             std::vector<std::reference_wrapper<ShadowContext>>> q;
  q.emplace(m_shadow);
  while(!q.empty()) {
    ShadowContext& shad = q.top();
    q.pop();

    Context& par = shad.m_shadowing.at(rroot);
    for(const auto& sc: shad.m_children) {
      sc.second->update(*this, rroot, par, sc.first, onadd);
      q.emplace(*sc.second);
    }
  }
}

Context& CollaborativeContext::unique(Context& ctx) noexcept {
  std::unique_lock<std::mutex> l(m_lock);
  auto it = m_shadowMap.find(ctx);
  if(it == m_shadowMap.end())
    util::log::fatal{} << "Attempt to unique a Context that has not been shadowed!";
  return it->second.second.m_unique.value();
}
