// vim:filetype=cpp:textwidth=120:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2014-2017 Christoph Schwering
// Licensed under the MIT license. See LICENSE file in the project root.
//
// Basic first-order formulas without any syntactic sugar. The atomic entities
// here are clauses, and the connectives are negation, disjunction, and
// existential quantifier.
//
// NF() rectifies a formula (that is, renames variables to make sure no variable
// occurs freely and bound or bound by two different quantifiers).

#ifndef LIMBO_FORMULA_H_
#define LIMBO_FORMULA_H_

#include <cassert>

#include <algorithm>
#include <list>
#include <memory>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include <limbo/clause.h>

#include <limbo/internal/iter.h>
#include <limbo/internal/intmap.h>
#include <limbo/internal/ints.h>
#include <limbo/internal/maybe.h>

namespace limbo {

class Formula {
 public:
  typedef internal::size_t size_t;
  typedef std::unique_ptr<Formula> Ref;
  struct SortOf { Symbol::Sort operator()(Term t) const { return t.sort(); } };
  typedef internal::IntMultiSet<Term, SortOf> SortedTermSet;
  typedef SortedTermSet::Bucket TermSet;
  typedef internal::IntMap<Symbol::Sort, size_t> SortCount;
  typedef unsigned int belief_level;
  enum Type { kAtomic, kNot, kOr, kExists, kKnow, kCons, kBel, kGuarantee };

  class Factory {
   public:
    Factory() = delete;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;
    Factory(Factory&&) = delete;
    Factory& operator=(Factory&&) = delete;

    inline static Ref Atomic(const Clause& c);
    inline static Ref Not(Ref alpha);
    inline static Ref Or(Ref lhs, Ref rhs);
    inline static Ref Exists(Term lhs, Ref rhs);
    inline static Ref Know(belief_level k, Ref alpha);
    inline static Ref Cons(belief_level k, Ref alpha);
    inline static Ref Bel(belief_level k, belief_level l, Ref alpha, Ref beta);
    inline static Ref Bel(belief_level k, belief_level l, Ref alpha, Ref beta, Ref not_alpha_or_beta);
    inline static Ref Guarantee(Ref alpha);
  };

  class Atomic;
  class Not;
  class Or;
  class Exists;
  class Know;
  class Cons;
  class Bel;
  class Guarantee;

  Formula(const Formula&) = delete;
  Formula& operator=(const Formula&) = delete;
  Formula(Formula&&) = default;
  Formula& operator=(Formula&&) = default;

  virtual ~Formula() {}

  virtual bool operator==(const Formula&) const = 0;
  bool operator!=(const Formula& that) const { return !(*this == that); }

  virtual Ref Clone() const = 0;

  Type type() const { return type_; }

  inline const Atomic&    as_atomic() const;
  inline const Not&       as_not() const;
  inline const Or&        as_or() const;
  inline const Exists&    as_exists() const;
  inline const Know&      as_know() const;
  inline const Cons&      as_cons() const;
  inline const Bel&       as_bel() const;
  inline const Guarantee& as_guarantee() const;

  const SortedTermSet& free_vars() const {
    if (!free_vars_) {
      free_vars_ = internal::Just(FreeVars());
    }
    return free_vars_.val;
  }

  virtual SortCount n_vars() const = 0;

  template<typename UnaryFunction>
  void SubstituteFree(UnaryFunction theta, Term::Factory* tf) {
    class FreeSubstitution : public ISubstitution {
     public:
      explicit FreeSubstitution(UnaryFunction func) : func_(func) {}
      internal::Maybe<Term> operator()(Term t) const override { return !bound(t) ? func_(t) : internal::Nothing; }
     private:
      UnaryFunction func_;
    };
    ISubstitute(FreeSubstitution(theta), tf);
  }

  template<typename UnaryFunction>
  void Traverse(UnaryFunction f) const {
    typedef typename internal::remove_const_ref<typename internal::arg<UnaryFunction>::template type<0>>::type arg_type;
    class Traversal : public ITraversal<arg_type> {
     public:
      explicit Traversal(UnaryFunction func) : func_(func) {}
      bool operator()(const arg_type& t) const override { return func_(t); }
     private:
      UnaryFunction func_;
    };
    ITraverse(Traversal(f));
  }

  Ref NF(Symbol::Factory* sf, Term::Factory* tf, bool distribute = true) const {
    Formula::Ref c = Clone();
    c->Rectify(sf, tf);
    c = c->Normalize(distribute);
    c = c->Flatten(0, sf, tf);
    c = c->Normalize(distribute);
    return Ref(std::move(c));
  }

  internal::Maybe<Clause> AsUnivClause() const { return AsUnivClause(0); }

  virtual bool objective() const = 0;
  virtual bool subjective() const = 0;
  virtual bool quantified_in() const = 0;
  virtual bool trivially_valid() const = 0;
  virtual bool trivially_invalid() const = 0;

 protected:
  class ISubstitution {
   public:
    virtual ~ISubstitution() {}
    virtual internal::Maybe<Term> operator()(Term t) const = 0;
    void Bind(Term t) const { bound_.insert(t); }
    void Unbind(Term t) const { bound_.erase(t); }
    bool bound(Term t) const { return bound_.contains(t); }
   private:
    mutable SortedTermSet bound_;
  };

  template<typename T>
  class ITraversal {
   public:
    virtual ~ITraversal() {}
    virtual bool operator()(const T& t) const = 0;
  };

  typedef std::unordered_map<Term, Term> TermMap;

  class QuantifierPrefix {
   public:
    void prepend_not() { prefix_.push_front(Element{kNot}); }
    void append_not() { prefix_.push_back(Element{kNot}); }
    void prepend_exists(Term x) { prefix_.push_front(Element{kExists, x}); }
    void append_exists(Term x) { prefix_.push_back(Element{kExists, x}); }

    size_t size() const { return prefix_.size(); }
    bool even() const {
      size_t n = 0;
      for (const auto& e : prefix_) {
        if (e.type == kNot) {
          ++n;
        }
      }
      return n % 2 == 0;
    }

    Ref PrependTo(Ref alpha) const;

   private:
    struct Element { Type type; Term x; };
    std::list<Element> prefix_;
  };

  explicit Formula(Type type) : type_(type) {}

  virtual SortedTermSet FreeVars() const = 0;

  virtual void ISubstitute(const ISubstitution&, Term::Factory*) = 0;
  virtual void ITraverse(const ITraversal<Term>&)    const = 0;
  virtual void ITraverse(const ITraversal<Literal>&) const = 0;
  virtual void ITraverse(const ITraversal<Clause>&)  const = 0;
  virtual void ITraverse(const ITraversal<Formula>&) const = 0;

  void Rectify(Symbol::Factory* sf, Term::Factory* tf) {
    TermMap tm;
    for (Term x : free_vars()) {
      tm.insert(std::make_pair(x, x));
    }
    // Rectify() renames every bound variable that also occurs free globally
    // somewhere in the formula or is bound by another quantifier to the left
    // of the current position.
    Rectify(&tm, sf, tf);
  }
  virtual void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) = 0;

  virtual std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const = 0;

  virtual Ref Normalize(bool distribute) const = 0;

  virtual Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const = 0;

  virtual internal::Maybe<Clause> AsUnivClause(size_t nots) const = 0;

 private:
  Type type_;
  mutable internal::Maybe<SortedTermSet> free_vars_ = internal::Nothing;
};

Formula::Ref Formula::QuantifierPrefix::PrependTo(Formula::Ref alpha) const {
  for (auto it = prefix_.rbegin(); it != prefix_.rend(); ++it) {
    assert(it->type == kNot || it->type == kExists);
    if (it->type == kNot) {
      alpha = Factory::Not(std::move(alpha));
    } else {
      alpha = Factory::Exists(it->x, std::move(alpha));
    }
  }
  return Ref(std::move(alpha));
}

class Formula::Atomic : public Formula {
 public:
  bool operator==(const Formula& that) const override { return type() == that.type() && c_ == that.as_atomic().c_; }

  Ref Clone() const override { return Factory::Atomic(c_); }

  const Clause& arg() const { return c_; }

  SortCount n_vars() const override {
    SortCount m;
    for (Term x : free_vars()) {
      ++m[x.sort()];
    }
    return m;
  }

  bool objective() const override { return true; }
  bool subjective() const override {
    return std::all_of(c_.begin(), c_.end(), [](Literal a) { return !a.lhs().function() && !a.rhs().function(); });
  }
  bool quantified_in() const override { return false; }
  bool trivially_valid() const override { return c_.valid(); }
  bool trivially_invalid() const override { return c_.invalid(); }

 protected:
  friend class Factory;

  explicit Atomic(const Clause& c) : Formula(Formula::kAtomic), c_(c) {}

  SortedTermSet FreeVars() const override {
    SortedTermSet ts;
    c_.Traverse([&ts](Term x) { if (x.variable()) ts.insert(x); return true; });
    return ts;
  }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override {
    c_ = c_.Substitute([&theta](Term t) { return theta(t); }, tf);
  }
  void ITraverse(const ITraversal<Term>& f)    const override { c_.Traverse([&f](Term t) { return f(t); }); }
  void ITraverse(const ITraversal<Literal>& f) const override { c_.Traverse([&f](Literal a) { return f(a); }); }
  void ITraverse(const ITraversal<Clause>& f)  const override { f(c_); }
  void ITraverse(const ITraversal<Formula>& f) const override { f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override {
    c_ = c_.Substitute([tm](Term t) {
      TermMap::const_iterator it;
      return t.variable() && ((it = tm->find(t)) != tm->end() && it->first != it->second)
          ? internal::Just(it->second) : internal::Nothing;
    }, tf);
  }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    return std::make_pair(QuantifierPrefix(), this);
  }

  Ref Normalize(bool distribute) const override { return Clone(); }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    // The following two expressions are equivalent provided that x1 ... xN do
    // not occur in t1 ... tN:
    // (1)  Fa x1 ... Fa xN (t1 != x1 || ... || tN != xN || c)
    // (2)  Ex x1 ... Ex xN (t1 == x1 && ... && tN == xN && c)
    // From the reasoner's point of view, (1) is preferable because it's a bigger
    // clause.
    // This method generates clauses of the form (1). However, when c is nested in
    // an odd number of negations, the result is equivalent to (2). In the special
    // case where c is a unit clause, we can still keep the clausal structure of
    // the transformed formula. For the following is equivalent: we negate the
    // literal in the unit clause, apply the transformation to the new unit
    // clause, and prepend another negation to the transformed formula.
    typedef std::unordered_set<Literal> LiteralSet;
    bool add_double_negation = nots % 2 == 1 && arg().unit();
    const Clause c = add_double_negation ? Clause(arg().first().flip()) : arg();
    LiteralSet queue(c.begin(), c.end());
    TermMap term_to_var;
    for (Literal a : queue) {
      if (!a.pos() && a.lhs().function() && a.rhs().variable()) {
        term_to_var[a.lhs()] = a.rhs();
      }
    }
    LiteralSet lits;
    QuantifierPrefix vars;
    while (!queue.empty()) {
      auto it = queue.begin();
      Literal a = *it;
      queue.erase(it);
      if (a.quasiprimitive() || (!a.lhs().function() && !a.rhs().function())) {
        lits.insert(a);
      } else if (a.rhs().function() &&
                 (!a.pos() || std::all_of(queue.begin(), queue.end(), [](Literal a) { return a.pos(); }))) {
        assert(a.lhs().function());
        Term old_t = a.lhs().arity() < a.rhs().arity() ? a.lhs() : a.rhs();
        Term new_t;
        TermMap::const_iterator it = term_to_var.find(old_t);
        if (it != term_to_var.end()) {
          new_t = it->second;
        } else {
          new_t = tf->CreateTerm(sf->CreateVariable(old_t.sort()));
          term_to_var[old_t] = new_t;
          vars.append_exists(new_t);
        }
        Literal new_a = a.Substitute(Term::Substitution(old_t, new_t), tf);
        Literal new_b = Literal::Neq(new_t, old_t);
        queue.insert(new_a);
        queue.insert(new_b);
      } else {
        assert(!a.lhs().quasiprimitive());
        for (Term arg : a.lhs().args()) {
          if (arg.function()) {
            Term old_arg = arg;
            Term new_arg;
            TermMap::const_iterator it = term_to_var.find(old_arg);
            if (it != term_to_var.end()) {
              new_arg = it->second;
            } else {
              new_arg = tf->CreateTerm(sf->CreateVariable(old_arg.sort()));
              term_to_var[old_arg] = new_arg;
              vars.append_exists(new_arg);
            }
            Literal new_a = a.Substitute(Term::Substitution(old_arg, new_arg), tf);
            Literal new_b = Literal::Neq(new_arg, old_arg);
            queue.insert(new_a);
            queue.insert(new_b);
            break;
          }
        }
      }
    }
    assert(lits.size() >= arg().size());
    assert(std::all_of(lits.begin(), lits.end(), [](Literal a) {
                       return a.quasiprimitive() || (!a.lhs().function() && !a.rhs().function()); }));
    if (vars.size() == 0) {
      return Clone();
    } else {
      if (!add_double_negation) {
        vars.prepend_not();
      }
      vars.append_not();
      return vars.PrependTo(Factory::Atomic(Clause(lits.begin(), lits.end())));
    }
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override {
    if (nots % 2 != 0 ||
        !std::all_of(c_.begin(), c_.end(), [](Literal a) {
                     return a.quasiprimitive() || (!a.lhs().function() && !a.rhs().function()); })) {
      return internal::Nothing;
    }
    return internal::Just(c_);
  }

 private:
  Clause c_;
};

class Formula::Or : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() && *alpha_ == *that.as_or().alpha_ && *beta_ == *that.as_or().beta_;
  }

  Ref Clone() const override { return Factory::Or(alpha_->Clone(), beta_->Clone()); }

  const Formula& lhs() const { return *alpha_; }
  const Formula& rhs() const { return *beta_; }

  SortCount n_vars() const override {
    SortCount m;
    for (Term x : free_vars()) {
      ++m[x.sort()];
    }
    m.Zip(alpha_->n_vars(), [](size_t a, size_t b) { return std::max(a, b); });
    m.Zip(beta_->n_vars(), [](size_t a, size_t b) { return std::max(a, b); });
    return m;
  }

  bool objective() const override { return alpha_->objective() && beta_->objective(); }
  bool subjective() const override { return alpha_->subjective() && beta_->subjective(); }
  bool quantified_in() const override { return alpha_->quantified_in() || beta_->quantified_in(); }
  bool trivially_valid() const override { return alpha_->trivially_valid() || beta_->trivially_valid(); }
  bool trivially_invalid() const override { return alpha_->trivially_invalid() && beta_->trivially_invalid(); }

 protected:
  friend class Factory;

  Or(Ref lhs, Ref rhs) : Formula(kOr), alpha_(std::move(lhs)), beta_(std::move(rhs)) {}

  SortedTermSet FreeVars() const override {
    SortedTermSet ts = alpha_->free_vars();
    for (Term x : beta_->free_vars().values()) {
      ts.insert(x);
    }
    return ts;
  }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override {
    alpha_->ISubstitute(theta, tf);
    beta_->ISubstitute(theta, tf);
  }
  void ITraverse(const ITraversal<Term>& f)    const override { alpha_->ITraverse(f); beta_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { alpha_->ITraverse(f); beta_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { alpha_->ITraverse(f); beta_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { alpha_->ITraverse(f); beta_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override {
    alpha_->Rectify(tm, sf, tf);
    beta_->Rectify(tm, sf, tf);
  }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    return std::make_pair(QuantifierPrefix(), this);
  }

  Ref Normalize(bool distribute) const override {
    Ref l = alpha_->Normalize(distribute);
    Ref r = beta_->Normalize(distribute);
    QuantifierPrefix lp;
    QuantifierPrefix rp;
    const Formula* ls;
    const Formula* rs;
    std::tie(lp, ls) = l->quantifier_prefix();
    std::tie(rp, rs) = r->quantifier_prefix();
    if (ls->type() == kAtomic && (lp.even() || ls->as_atomic().arg().unit()) &&
        rs->type() == kAtomic && (rp.even() || rs->as_atomic().arg().unit())) {
      Clause lc = ls->as_atomic().arg();
      Clause rc = rs->as_atomic().arg();
      if (!lp.even()) {
        lp.append_not();
        lc = Clause(lc.first().flip());
      }
      if (!rp.even()) {
        rp.append_not();
        rc = Clause(rc.first().flip());
      }
      const auto lits = internal::join_cranges(lc, rc);
      return lp.PrependTo(rp.PrependTo(Factory::Atomic(Clause(lits.begin(), lits.end()))));
    } else {
      return Factory::Or(std::move(l), std::move(r));
    }
  }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    return Factory::Or(alpha_->Flatten(nots, sf, tf), beta_->Flatten(nots, sf, tf));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override {
    if (nots % 2 != 0) {
      return internal::Nothing;
    }
    const internal::Maybe<Clause> c1 = alpha_->AsUnivClause(nots);
    const internal::Maybe<Clause> c2 = beta_->AsUnivClause(nots);
    if (!c1 || !c2) {
      return internal::Nothing;
    }
    const auto r = internal::join_cranges(c1.val, c2.val);
    return internal::Just(Clause(r.begin(), r.end()));
  }

 private:
  Ref alpha_;
  Ref beta_;
};

class Formula::Exists : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() && x_ == that.as_exists().x_ && *alpha_ == *that.as_exists().alpha_;
  }

  Ref Clone() const override { return Factory::Exists(x_, alpha_->Clone()); }

  Term x() const { return x_; }
  const Formula& arg() const { return *alpha_; }

  SortCount n_vars() const override { return alpha_->n_vars(); }

  bool objective() const override { return alpha_->objective(); }
  bool subjective() const override { return alpha_->subjective(); }
  bool quantified_in() const override { return alpha_->quantified_in(); }
  bool trivially_valid() const override { return alpha_->trivially_valid(); }
  bool trivially_invalid() const override { return alpha_->trivially_invalid(); }

 protected:
  friend class Factory;

  Exists(Term x, Ref alpha) : Formula(kExists), x_(x), alpha_(std::move(alpha)) {}

  SortedTermSet FreeVars() const override { SortedTermSet ts = alpha_->free_vars(); ts.erase(x_); return ts; }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override {
    theta.Bind(x_);
    alpha_->ISubstitute(theta, tf);
    theta.Unbind(x_);
  }
  void ITraverse(const ITraversal<Term>& f)    const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { alpha_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override {
    TermMap::const_iterator it = tm->find(x_);
    if (it != tm->end()) {
      const Term old_x = x_;
      const Term new_x = tf->CreateTerm(sf->CreateVariable(old_x.sort()));
      tm->insert(it, std::make_pair(old_x, new_x));
      x_ = new_x;
    } else {
      tm->insert(it, std::make_pair(x_, x_));
    }
    alpha_->Rectify(tm, sf, tf);
  }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    auto p = alpha_->quantifier_prefix();
    p.first.prepend_exists(x_);
    return p;
  }

  Ref Normalize(bool distribute) const override {
    const SortedTermSet& ts = alpha_->free_vars();
    Ref alpha = alpha_->Normalize(distribute);
    if (ts.contains(x_)) {
      return Factory::Exists(x_, std::move(alpha));
    } else {
      return alpha;
    }
  }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    return Factory::Exists(x_, alpha_->Flatten(nots, sf, tf));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override {
    if (nots % 2 == 0) {
      return internal::Nothing;
    }
    return alpha_->AsUnivClause(nots);
  }

 private:
  Term x_;
  Ref alpha_;
};

class Formula::Not : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() && *alpha_ == *that.as_not().alpha_;
  }

  Ref Clone() const override { return Factory::Not(alpha_->Clone()); }

  const Formula& arg() const { return *alpha_; }

  SortCount n_vars() const override { return alpha_->n_vars(); }

  bool objective() const override { return alpha_->objective(); }
  bool subjective() const override { return alpha_->subjective(); }
  bool quantified_in() const override { return false; }
  bool trivially_valid() const override { return alpha_->trivially_invalid(); }
  bool trivially_invalid() const override { return alpha_->trivially_valid(); }

 protected:
  friend class Factory;

  explicit Not(Ref alpha) : Formula(kNot), alpha_(std::move(alpha)) {}

  SortedTermSet FreeVars() const override { return alpha_->free_vars(); }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override { alpha_->ISubstitute(theta, tf); }
  void ITraverse(const ITraversal<Term>& f)    const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { alpha_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override { alpha_->Rectify(tm, sf, tf); }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    auto p = alpha_->quantifier_prefix();
    p.first.prepend_not();
    return p;
  }

  Ref Normalize(bool distribute) const override {
    switch (alpha_->type()) {
      case kAtomic: {
        const Clause& c = arg().as_atomic().arg();
        if (c.unit()) {
          return Factory::Atomic(Clause(c.first().flip()));
        } else {
          return Clone();
        }
      }
      case kNot:
        return arg().as_not().arg().Normalize(distribute);
      case kOr:
        return Factory::Not(arg().Normalize(distribute));
      case kExists: {
        Term x = arg().as_exists().x();
        const Formula& alpha = arg().as_exists().arg();
        return Factory::Not(Factory::Exists(x, alpha.Normalize(distribute)));
      }
      case kKnow:
      case kCons:
      case kBel:
      case kGuarantee: {
        return Factory::Not(arg().Normalize(distribute));
      }
    }
    throw;
  }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    return Factory::Not(alpha_->Flatten(nots + 1, sf, tf));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override {
    return alpha_->AsUnivClause(nots + 1);
  }

 private:
  Ref alpha_;
};

class Formula::Know : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() && *alpha_ == *that.as_know().alpha_;
  }

  Ref Clone() const override { return Factory::Know(k_, alpha_->Clone()); }

  belief_level k() const { return k_; }
  const Formula& arg() const { return *alpha_; }

  SortCount n_vars() const override { return alpha_->n_vars(); }

  bool objective() const override { return false; }
  bool subjective() const override { return true; }
  bool quantified_in() const override { return !free_vars().all_empty(); }
  bool trivially_valid() const override { return alpha_->trivially_valid(); }
  bool trivially_invalid() const override { return false; }

 protected:
  friend class Factory;

  Know(belief_level k, Ref alpha) : Formula(kKnow), k_(k), alpha_(std::move(alpha)) {}

  SortedTermSet FreeVars() const override { return alpha_->free_vars(); }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override { alpha_->ISubstitute(theta, tf); }
  void ITraverse(const ITraversal<Term>& f)    const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { alpha_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override { alpha_->Rectify(tm, sf, tf); }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    return std::make_pair(QuantifierPrefix(), this);
  }

  Ref Normalize(bool distribute) const override {
    Ref alpha = alpha_->Normalize(distribute);
    if (distribute) {
      return DistK(k_, std::move(alpha));
    } else {
      return Factory::Know(k_, std::move(alpha));
    }
  }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    Formula::Ref alpha = alpha_->Flatten(0, sf, tf);
    return Factory::Know(k_, std::move(alpha));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override { return internal::Nothing; }

 private:
  static Ref DistK(belief_level k, Ref alpha) {
    if (alpha->type() == kNot) {
      const Formula& beta = alpha->as_not().arg();
      switch (beta.type()) {
        case kAtomic: {
          const Clause& c = beta.as_atomic().arg();
          if (c.size() == 1) {
            return Factory::Know(k, Factory::Atomic(Clause{c[0].flip()}));
          } else if (c.size() >= 2) {
            Ref gamma;
            for (Literal a : c) {
              Ref delta = Factory::Know(k, Factory::Atomic(Clause{a.flip()}));
              if (!gamma) {
                gamma = std::move(std::move(delta));
              } else {
                gamma = Factory::Or(std::move(gamma), std::move(delta));
              }
            }
            gamma = Factory::Not(std::move(gamma));
            return gamma;
          }
          break;
        }
        case kNot:
          return DistK(k, beta.as_not().arg().Clone());
        case kOr:
          return Factory::Not(Factory::Or(Factory::Not(DistK(k, Factory::Not(beta.as_or().lhs().Clone()))),
                                          Factory::Not(DistK(k, Factory::Not(beta.as_or().rhs().Clone())))));
        case kExists:
          return Factory::Not(Factory::Exists(beta.as_exists().x(),
                                              Factory::Not(DistK(k, Factory::Not(beta.as_exists().arg().Clone())))));
        case kKnow:
        case kCons:
        case kBel:
        case kGuarantee:
          break;
      }
    }
    return Factory::Know(k, std::move(alpha));
  }

  belief_level k_;
  Ref alpha_;
};

class Formula::Cons : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() && *alpha_ == *that.as_cons().alpha_;
  }

  Ref Clone() const override { return Factory::Cons(k_, alpha_->Clone()); }

  belief_level k() const { return k_; }
  const Formula& arg() const { return *alpha_; }

  SortCount n_vars() const override { return alpha_->n_vars(); }

  bool objective() const override { return false; }
  bool subjective() const override { return true; }
  bool quantified_in() const override { return !free_vars().all_empty(); }
  bool trivially_valid() const override { return false; }
  bool trivially_invalid() const override { return alpha_->trivially_invalid(); }

 protected:
  friend class Factory;

  Cons(belief_level k, Ref alpha) : Formula(kCons), k_(k), alpha_(std::move(alpha)) {}

  SortedTermSet FreeVars() const override { return alpha_->free_vars(); }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override { alpha_->ISubstitute(theta, tf); }
  void ITraverse(const ITraversal<Term>& f)    const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { alpha_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override { alpha_->Rectify(tm, sf, tf); }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    return std::make_pair(QuantifierPrefix(), this);
  }

  Ref Normalize(bool distribute) const override {
    Ref alpha = alpha_->Normalize(distribute);
    if (distribute) {
      return DistM(k_, std::move(alpha));
    } else {
      return Factory::Cons(k_, std::move(alpha));
    }
  }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    Formula::Ref alpha = alpha_->Flatten(0, sf, tf);
    return Factory::Cons(k_, std::move(alpha));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override { return internal::Nothing; }

 private:
  static Ref DistM(belief_level k, Ref alpha) {
    switch (alpha->type()) {
      case kAtomic: {
        const Clause& c = alpha->as_atomic().arg();
        if (c.size() >= 2) {
          Ref gamma;
          for (Literal a : c) {
            Ref delta = Factory::Know(k, Factory::Atomic(Clause{a.flip()}));
            if (!gamma) {
              gamma = std::move(std::move(delta));
            } else {
              gamma = Factory::Or(std::move(gamma), std::move(delta));
            }
          }
          return gamma;
        }
        break;
      }
      case kNot:
        break;
      case kOr:
        return Factory::Or(DistM(k, alpha->as_or().lhs().Clone()),
                           DistM(k, alpha->as_or().rhs().Clone()));
      case kExists:
        return Factory::Exists(alpha->as_exists().x(), DistM(k, alpha->as_exists().arg().Clone()));
      case kKnow:
      case kCons:
      case kBel:
      case kGuarantee:
        break;
    }
    return Factory::Cons(k, std::move(alpha));
  }

  belief_level k_;
  Ref alpha_;
};

class Formula::Bel : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() &&
        *ante_ == *that.as_bel().ante_ &&
        *not_ante_or_conse_ == *that.as_bel().not_ante_or_conse_;
  }

  Ref Clone() const override {
    return Factory::Bel(k_, l_, ante_->Clone(), conse_->Clone(), not_ante_or_conse_->Clone());
  }

  belief_level k() const { return k_; }
  belief_level l() const { return l_; }
  const Formula& antecedent() const { return *ante_; }
  const Formula& consequent() const { return *conse_; }
  const Formula& not_antecedent_or_consequent() const { return *not_ante_or_conse_; }

  SortCount n_vars() const override { return not_ante_or_conse_->n_vars(); }

  bool objective() const override { return false; }
  bool subjective() const override { return true; }
  bool quantified_in() const override { return !free_vars().all_empty(); }
  bool trivially_valid() const override { return not_ante_or_conse_->trivially_valid(); }
  bool trivially_invalid() const override { return false; }

 protected:
  friend class Factory;

  Bel(belief_level k, belief_level l, Ref antecedent, Ref consequent) :
      Formula(kBel),
      k_(k),
      l_(l),
      ante_(antecedent->Clone()),
      conse_(consequent->Clone()),
      not_ante_or_conse_(Factory::Or(Factory::Not(std::move(antecedent)), std::move(consequent))) {}

  SortedTermSet FreeVars() const override { return not_ante_or_conse_->free_vars(); }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override {
    ante_->ISubstitute(theta, tf);
    conse_->ISubstitute(theta, tf);
    not_ante_or_conse_->ISubstitute(theta, tf);
  }
  void ITraverse(const ITraversal<Term>& f)    const override { ante_->ITraverse(f); conse_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { ante_->ITraverse(f); conse_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { ante_->ITraverse(f); conse_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { ante_->ITraverse(f); conse_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override {
    ante_->Rectify(tm, sf, tf);
    conse_->Rectify(tm, sf, tf);
    not_ante_or_conse_->Rectify(tm, sf, tf);
  }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    return std::make_pair(QuantifierPrefix(), this);
  }

  Ref Normalize(bool distribute) const override {
    return Factory::Bel(k_, l_, ante_->Normalize(distribute), conse_->Normalize(distribute),
                        not_ante_or_conse_->Normalize(distribute));
  }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    Formula::Ref ante = ante_->Flatten(0, sf, tf);
    Formula::Ref conse = conse_->Flatten(0, sf, tf);
    Formula::Ref not_ante_or_conse = not_ante_or_conse_->Flatten(0, sf, tf);
    return Factory::Bel(k_, l_, std::move(ante), std::move(conse), std::move(not_ante_or_conse));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override { return internal::Nothing; }

 private:
  Bel(belief_level k, belief_level l, Ref antecedent, Ref consequent, Ref not_antecedent_or_consequent) :
      Formula(kBel),
      k_(k),
      l_(l),
      ante_(std::move(antecedent)),
      conse_(std::move(consequent)),
      not_ante_or_conse_(std::move(not_antecedent_or_consequent)) {}

  belief_level k_;
  belief_level l_;
  Ref ante_;
  Ref conse_;
  Ref not_ante_or_conse_;
};

class Formula::Guarantee : public Formula {
 public:
  bool operator==(const Formula& that) const override {
    return type() == that.type() && *alpha_ == *that.as_guarantee().alpha_;
  }

  Ref Clone() const override { return Factory::Guarantee(alpha_->Clone()); }

  const Formula& arg() const { return *alpha_; }

  SortCount n_vars() const override { return alpha_->n_vars(); }

  bool objective() const override { return alpha_->objective(); }
  bool subjective() const override { return alpha_->subjective(); }
  bool quantified_in() const override { return alpha_->quantified_in(); }
  bool trivially_valid() const override { return alpha_->trivially_valid(); }
  bool trivially_invalid() const override { return alpha_->trivially_invalid(); }

 protected:
  friend class Factory;

  explicit Guarantee(Ref alpha) :
      Formula(kGuarantee),
      alpha_(std::move(alpha)) {}

  SortedTermSet FreeVars() const override { return alpha_->free_vars(); }

  void ISubstitute(const ISubstitution& theta, Term::Factory* tf) override { alpha_->ISubstitute(theta, tf); }
  void ITraverse(const ITraversal<Term>& f)    const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Literal>& f) const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Clause>& f)  const override { alpha_->ITraverse(f); }
  void ITraverse(const ITraversal<Formula>& f) const override { alpha_->ITraverse(f); f(*this); }

  void Rectify(TermMap* tm, Symbol::Factory* sf, Term::Factory* tf) override { alpha_->Rectify(tm, sf, tf); }

  std::pair<QuantifierPrefix, const Formula*> quantifier_prefix() const override {
    return std::make_pair(QuantifierPrefix(), this);
  }

  Ref Normalize(bool distribute) const override { return Factory::Guarantee(alpha_->Normalize(distribute)); }

  Ref Flatten(size_t nots, Symbol::Factory* sf, Term::Factory* tf) const override {
    return Factory::Guarantee(alpha_->Flatten(nots, sf, tf));
  }

  internal::Maybe<Clause> AsUnivClause(size_t nots) const override { return internal::Nothing; }

 private:
  Ref alpha_;
};

Formula::Ref Formula::Factory::Atomic(const Clause& c)   { return Ref(new class Atomic(c)); }
Formula::Ref Formula::Factory::Not(Ref alpha)            { return Ref(new class Not(std::move(alpha))); }
Formula::Ref Formula::Factory::Or(Ref lhs, Ref rhs)    { return Ref(new class Or(std::move(lhs), std::move(rhs))); }
Formula::Ref Formula::Factory::Exists(Term x, Ref alpha) { return Ref(new class Exists(x, std::move(alpha))); }
Formula::Ref Formula::Factory::Know(belief_level k, Ref alpha) { return Ref(new class Know(k, std::move(alpha))); }
Formula::Ref Formula::Factory::Cons(belief_level k, Ref alpha) { return Ref(new class Cons(k, std::move(alpha))); }
Formula::Ref Formula::Factory::Bel(belief_level k, belief_level l, Ref alpha, Ref beta) {
  return Ref(new class Bel(k, l, std::move(alpha), std::move(beta)));
}
Formula::Ref Formula::Factory::Bel(belief_level k, belief_level l, Ref alpha, Ref beta, Ref not_alpha_or_beta) {
  return Ref(new class Bel(k, l, std::move(alpha), std::move(beta), std::move(not_alpha_or_beta)));
}
Formula::Ref Formula::Factory::Guarantee(Ref alpha) { return Ref(new class Guarantee(std::move(alpha))); }

inline const class Formula::Atomic& Formula::as_atomic() const {
  assert(type_ == kAtomic);
  return *dynamic_cast<const class Atomic*>(this);
}
inline const class Formula::Not& Formula::as_not() const {
  assert(type_ == kNot);
  return *dynamic_cast<const class Not*>(this);
}
inline const class Formula::Or& Formula::as_or() const {
  assert(type_ == kOr);
  return *dynamic_cast<const class Or*>(this);
}
inline const class Formula::Exists& Formula::as_exists() const {
  assert(type_ == kExists);
  return *dynamic_cast<const class Exists*>(this);
}
inline const class Formula::Know& Formula::as_know() const {
  assert(type_ == kKnow);
  return *dynamic_cast<const class Know*>(this);
}
inline const class Formula::Cons& Formula::as_cons() const {
  assert(type_ == kCons);
  return *dynamic_cast<const class Cons*>(this);
}
inline const class Formula::Bel& Formula::as_bel() const {
  assert(type_ == kBel);
  return *dynamic_cast<const class Bel*>(this);
}
inline const class Formula::Guarantee& Formula::as_guarantee() const {
  assert(type_ == kGuarantee);
  return *dynamic_cast<const class Guarantee*>(this);
}

}  // namespace limbo

#endif  // LIMBO_FORMULA_H_

