// vim:filetype=cpp:textwidth=120:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2016-2017 Christoph Schwering
// Licensed under the MIT license. See LICENSE file in the project root.
//
// Context objects store and create symbols, terms, and allow for textual
// representation, and encapsulate a Solver object.
//
// Results are announced through the Logger functor, which needs to implement
// operator() for the structs defined in Logger. Logger itself is a minimal
// implementation of a Logger, which ignores all log data.

#ifndef LIMBO_FORMAT_PDL_CONTEXT_H_
#define LIMBO_FORMAT_PDL_CONTEXT_H_

#include <iostream>
#include <map>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

#include <limbo/term.h>
#include <limbo/formula.h>
#include <limbo/kb.h>

#include <limbo/format/output.h>

namespace limbo {
namespace format {
namespace pdl {

struct DefaultLogger {
  struct LogData {};

  struct RegisterData {
    explicit RegisterData(const std::string& id) : id(id) {}
    const std::string id;
  };

  struct RegisterSortData : public RegisterData {
    explicit RegisterSortData(const std::string& id) : RegisterData(id) {}
  };

  struct RegisterNameData : public RegisterData {
    RegisterNameData(const std::string& id, const std::string& sort_id) : RegisterData(id), sort_id(sort_id) {}
    const std::string sort_id;
  };

  struct RegisterVariableData : public RegisterData {
    RegisterVariableData(const std::string& id, const std::string& sort_id) : RegisterData(id), sort_id(sort_id) {}
    const std::string sort_id;
  };

  struct RegisterFunctionData : public RegisterData {
    RegisterFunctionData(const std::string& id, Symbol::Arity arity, const std::string& sort_id)
        : RegisterData(id), arity(arity), sort_id(sort_id) {}
    Symbol::Arity arity;
    const std::string sort_id;
  };

  struct RegisterMetaVariableData : public RegisterData {
    RegisterMetaVariableData(const std::string& id, const Term t) : RegisterData(id), term(t) {}
    const Term term;
  };

  struct RegisterFormulaData : public RegisterData {
    RegisterFormulaData(const std::string& id, const Formula& phi) : RegisterData(id), phi(phi.Clone()) {}
    const Formula::Ref phi;
  };

  struct UnregisterData : public LogData {
    explicit UnregisterData(const std::string& id) : id(id) {}
    const std::string id;
  };

  struct UnregisterMetaVariableData : public UnregisterData {
    explicit UnregisterMetaVariableData(const std::string& id) : UnregisterData(id) {}
  };

  struct AddToKbData : public LogData {
    AddToKbData(const Formula& alpha, bool ok) : alpha(alpha), ok(ok) {}
    const Formula& alpha;
    const bool ok;
  };

  struct QueryData : public LogData {
    QueryData(const KnowledgeBase& kb, const Formula& phi, bool yes) :
        kb(kb), phi(phi.Clone()), yes(yes) {}
    const KnowledgeBase& kb;
    const Formula::Ref phi;
    const bool yes;
  };

  void operator()(const LogData& d) const {}
};

struct DefaultCallback {
  template<typename T>
  void operator()(T* ctx, const std::string& proc, const std::vector<Term>& args) const {}
};

template<typename Logger = DefaultLogger, typename Callback = DefaultCallback>
class Context {
 public:
  explicit Context(Logger p = Logger(), Callback c = Callback()) : logger_(p), callback_(c), kb_(sf(), tf()) {}

  void Call(const std::string& proc, const std::vector<Term>& args) {
    callback_(this, proc, args);
  }

  Symbol::Sort CreateSort() { return sf()->CreateSort(); }
  Term CreateVariable(Symbol::Sort sort) { return tf()->CreateTerm(sf()->CreateVariable(sort)); }
  Term CreateName(Symbol::Sort sort) { return tf()->CreateTerm(sf()->CreateName(sort)); }
  Symbol CreateFunction(Symbol::Sort sort, Symbol::Arity arity) { return sf()->CreateFunction(sort, arity); }
  Term CreateTerm(Symbol symbol, const std::vector<Term>& args) { return tf()->CreateTerm(symbol, args); }

  bool IsRegisteredSort(const std::string& id) const { return sorts_.Registered(id); }
  bool IsRegisteredVariable(const std::string& id) const { return vars_.Registered(id); }
  bool IsRegisteredName(const std::string& id) const { return names_.Registered(id); }
  bool IsRegisteredFunction(const std::string& id) const { return funs_.Registered(id); }
  bool IsRegisteredMetaVariable(const std::string& id) const { return meta_vars_.Registered(id); }
  bool IsRegisteredFormula(const std::string& id) const { return formulas_.Registered(id); }
  bool IsRegisteredTerm(const std::string& id) const {
    return IsRegisteredVariable(id) || IsRegisteredName(id) || IsRegisteredFunction(id) || IsRegisteredMetaVariable(id);
  }

  Symbol::Sort LookupSort(const std::string& id) const { return sorts_.Find(id); }
  Term LookupVariable(const std::string& id) const { return vars_.Find(id); }
  Term LookupName(const std::string& id) const { return names_.Find(id); }
  Symbol LookupFunction(const std::string& id) const { return funs_.Find(id); }
  Term LookupMetaVariable(const std::string& id) const { return meta_vars_.Find(id); }
  const Formula& LookupFormula(const std::string& id) const { return *formulas_.Find(id); }

  void RegisterSort(const std::string& id) {
    const Symbol::Sort sort = CreateSort();
    limbo::format::RegisterSort(sort, "");
    sorts_.Register(id, sort);
    logger_(DefaultLogger::RegisterSortData(id));
  }

  void RegisterVariable(const std::string& id, const std::string& sort_id) {
    if (IsRegisteredVariable(id))
      throw std::domain_error(id);
    const Symbol::Sort sort = LookupSort(sort_id);
    const Term var = CreateVariable(sort);
    vars_.Register(id, var);
    limbo::format::RegisterSymbol(var.symbol(), id);
    logger_(DefaultLogger::RegisterVariableData(id, sort_id));
  }

  void RegisterName(const std::string& id, const std::string& sort_id) {
    if (IsRegisteredName(id))
      throw std::domain_error(id);
    const Symbol::Sort sort = LookupSort(sort_id);
    const Term name = CreateName(sort);
    names_.Register(id, name);
    limbo::format::RegisterSymbol(name.symbol(), id);
    logger_(DefaultLogger::RegisterNameData(id, sort_id));
  }

  void RegisterFunction(const std::string& id, int arity, const std::string& sort_id) {
    if (IsRegisteredFunction(id))
      throw std::domain_error(id);
    const Symbol::Sort sort = LookupSort(sort_id);
    const Symbol fun = CreateFunction(sort, arity);
    funs_.Register(id, fun);
    limbo::format::RegisterSymbol(fun, id);
    logger_(DefaultLogger::RegisterFunctionData(id, arity, sort_id));
  }

  void RegisterMetaVariable(const std::string& id, Term t) {
    if (IsRegisteredMetaVariable(id))
      throw std::domain_error(id);
    meta_vars_.Register(id, t);
    logger_(DefaultLogger::RegisterMetaVariableData(id, t));
  }

  void RegisterFormula(const std::string& id, const Formula& phi) {
    formulas_.Register(id, phi.Clone());
    logger_(DefaultLogger::RegisterFormulaData(id, phi));
  }

  void UnregisterMetaVariable(const std::string& id) {
    if (!IsRegisteredMetaVariable(id))
      throw std::domain_error(id);
    meta_vars_.Unregister(id);
    logger_(DefaultLogger::UnregisterMetaVariableData(id));
  }

  void set_distribute(bool b) { distribute_ = b; }
  bool distribute() const { return distribute_; }

  bool AddToKb(const Formula& alpha) {
    const bool ok = kb_.Add(alpha);
    logger_(DefaultLogger::AddToKbData(alpha, ok));
    return ok;
  }

  bool Query(const Formula& alpha) {
    const bool yes = kb_.Entails(alpha, distribute_);
    logger_(DefaultLogger::QueryData(kb_, alpha, yes));
    return yes;
  }

  KnowledgeBase& kb() { return kb_; }
  const KnowledgeBase& kb() const { return kb_; }

  Symbol::Factory* sf() { return Symbol::Factory::Instance(); }
  Term::Factory* tf() { return Term::Factory::Instance(); }

  const Logger& logger() const { return logger_; }
        Logger* logger()       { return &logger_; }

  const Callback& callback() const { return &callback_; }
        Callback* callback()       { return &callback_; }

 private:
  template<typename T>
  class Registry {
   public:
    bool Registered(const std::string& id) const { auto it = r_.find(id); return it != r_.end(); }
    void Register(const std::string& id, const T& val) { r_.insert(std::make_pair(id, val)); }
    void Register(const std::string& id, T&& val) { Unregister(id); r_.emplace(id, std::forward<T>(val)); }
    void Unregister(const std::string& id) { auto it = r_.find(id); if (it != r_.end()) { r_.erase(it); } }
    const T& Find(const std::string& id) const { auto it = r_.find(id); return it->second; }
   private:
    std::map<std::string, T> r_;
  };

  Logger                 logger_;
  Callback               callback_;
  Registry<Symbol::Sort> sorts_;
  Registry<Term>         vars_;
  Registry<Term>         names_;
  Registry<Symbol>       funs_;
  Registry<Term>         meta_vars_;
  Registry<Formula::Ref> formulas_;
  KnowledgeBase          kb_;
  bool                   distribute_ = true;
};

}  // namespace pdl
}  // namespace format
}  // namespace limbo

#endif  // LIMBO_FORMAT_PDL_CONTEXT_H_

