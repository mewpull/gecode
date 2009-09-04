/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *
 *  Last modified:
 *     $Date: 2006-12-11 03:27:31 +1100 (Mon, 11 Dec 2006) $ by $Author: schulte $
 *     $Revision: 4024 $
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <gecode/flatzinc/registry.hh>
#include <gecode/kernel.hh>
#include <gecode/int.hh>
#include <gecode/scheduling.hh>
#include <gecode/minimodel.hh>
#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#include <gecode/flatzinc/flatzinc.hh>

using namespace Gecode;

Registry registry;

void
Registry::post(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
  std::map<std::string,poster>::iterator i = r.find(ce.id);
  if (i == r.end()) {
    throw FlatZinc::Error("Registry",
      std::string("Constraint ")+ce.id+" not found");
  }
  i->second(s, ce, ann);
}

void
Registry::add(const std::string& id, poster p) {
  r[id] = p;
}

namespace {
  
  IntConLevel ann2icl(AST::Node* ann) {
    if (ann) {
      if (ann->hasAtom("val"))
        return ICL_VAL;
      if (ann->hasAtom("domain"))
        return ICL_DOM;
      if (ann->hasAtom("bounds") ||
          ann->hasAtom("boundsR") ||
          ann->hasAtom("boundsD") ||
          ann->hasAtom("boundsZ"))
        return ICL_BND;
    }
    return ICL_DEF;
  }
  
  inline IntRelType
  swap(IntRelType irt) {
    switch (irt) {
    case IRT_LQ: return IRT_GQ;
    case IRT_LE: return IRT_GR;
    case IRT_GQ: return IRT_LQ;
    case IRT_GR: return IRT_LE;
    default:     return irt;
    }
  }

  inline IntRelType
  neg(IntRelType irt) {
    switch (irt) {
    case IRT_EQ: return IRT_NQ;
    case IRT_NQ: return IRT_EQ;
    case IRT_LQ: return IRT_GR;
    case IRT_LE: return IRT_GQ;
    case IRT_GQ: return IRT_LE;
    case IRT_GR:
    default:
      assert(irt == IRT_GR);
    }
    return IRT_LQ;
  }

  inline IntArgs arg2intargs(AST::Node* arg, int offset = 0) {
    AST::Array* a = arg->getArray();
    IntArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = 0;
    for (int i=a->a.size(); i--;)
      ia[i+offset] = a->a[i]->getInt();
    return ia;
  }

  inline IntArgs arg2boolargs(AST::Node* arg, int offset = 0) {
    AST::Array* a = arg->getArray();
    IntArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = 0;
    for (int i=a->a.size(); i--;)
      ia[i+offset] = a->a[i]->getBool();
    return ia;
  }

  inline IntSetArgs arg2intsetargs(AST::Node* arg, int offset = 0) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      IntSetArgs emptyIa(0);
      return emptyIa;
    }
    IntSetArgs ia(a->a.size()+offset);      
    for (int i=offset; i--;)
      ia[i] = IntSet::empty;
    for (int i=a->a.size(); i--;) {
      AST::SetLit* sl = a->a[i]->getSet();
      if (sl->interval) {
        ia[i+offset] = IntSet(sl->min, sl->max);
      } else {
        int* is =
          heap.alloc<int>(static_cast<unsigned long int>(sl->s.size()));
        for (int j=sl->s.size(); j--; )
          is[j] = sl->s[j];
        ia[i+offset] = IntSet(is, sl->s.size());
        heap.free(is,static_cast<unsigned long int>(sl->s.size()));
      }
    }
    return ia;
  }
  
  inline IntVarArgs arg2intvarargs(FlatZincGecode& s, AST::Node* arg,
                                   int offset = 0) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      IntVarArgs emptyIa(0);
      return emptyIa;
    }
    IntVarArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = IntVar(s, 0, 0);
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isIntVar()) {
        ia[i+offset] = s.iv[a->a[i]->getIntVar()];        
      } else {
        int value = a->a[i]->getInt();
        IntVar iv(s, value, value);
        ia[i+offset] = iv;        
      }
    }
    return ia;
  }

  inline BoolVarArgs arg2boolvarargs(FlatZincGecode& s, AST::Node* arg,
                                     int offset = 0) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      BoolVarArgs emptyIa(0);
      return emptyIa;
    }
    BoolVarArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = BoolVar(s, 0, 0);
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isBool()) {
        bool value = a->a[i]->getBool();
        BoolVar iv(s, value, value);
        ia[i+offset] = iv;
      } else {
        ia[i+offset] = s.bv[a->a[i]->getBoolVar()];
      }
    }
    return ia;
  }

#ifdef GECODE_HAS_SET_VARS
  SetVar getSetVar(FlatZincGecode& s, AST::Node* n) {
    SetVar x0;
    if (!n->isSetVar()) {
      AST::SetLit* sl = n->getSet();
      if (sl->interval) {
        IntSet d(sl->min, sl->max);
        x0 = SetVar(s, d, d);        
      } else {
        Region re(s);
        int* is = re.alloc<int>(static_cast<unsigned long int>(sl->s.size()));
        for (int i=sl->s.size(); i--; )
          is[i] = sl->s[i];
        IntSet d(is, sl->s.size());
        x0 = SetVar(s, d, d);
      }
    } else {
      x0 = s.sv[n->getSetVar()];
    }
    return x0;
  }

  inline SetVarArgs arg2setvarargs(FlatZincGecode& s, AST::Node* arg,
                                   int offset = 0) {
    AST::Array* a = arg->getArray();
    if (a->a.size() == 0) {
      SetVarArgs emptyIa(0);
      return emptyIa;
    }
    SetVarArgs ia(a->a.size()+offset);
    for (int i=offset; i--;)
      ia[i] = SetVar(s, IntSet::empty, IntSet::empty);
    for (int i=a->a.size(); i--;) {
      ia[i+offset] = getSetVar(s, a->a[i]);
    }
    return ia;
  }
#endif

  BoolVar getBoolVar(FlatZincGecode& s, AST::Node* n) {
    BoolVar x0;
    if (n->isBool()) {
      x0 = BoolVar(s, n->getBool(), n->getBool());
    }
    else {
      x0 = s.bv[n->getBoolVar()];
    }
    return x0;
  }

  IntVar getIntVar(FlatZincGecode& s, AST::Node* n) {
    IntVar x0;
    if (n->isIntVar()) {
      x0 = s.iv[n->getIntVar()];
    } else {
      x0 = IntVar(s, n->getInt(), n->getInt());            
    }
    return x0;
  }

  void p_distinct(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs va = arg2intvarargs(s, ce[0]);
    distinct(s, va, ann2icl(ann));    
  }
  void p_distinctOffset(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs va = arg2intvarargs(s, ce[1]);
    AST::Array* offs = ce.args->a[0]->getArray();
    IntArgs oa(offs->a.size());
    for (int i=offs->a.size(); i--; ) {
      oa[i] = offs->a[i]->getInt();    
    }
    distinct(s, oa, va, ann2icl(ann));
  }

  void p_int_CMP(FlatZincGecode& s, IntRelType irt, const ConExpr& ce, 
                 AST::Node* ann) {
    if (ce[0]->isIntVar()) {
      if (ce[1]->isIntVar()) {
        rel(s, getIntVar(s, ce[0]), irt, getIntVar(s, ce[1]), 
            ann2icl(ann));
      } else {
        rel(s, getIntVar(s, ce[0]), irt, ce[1]->getInt(), ann2icl(ann));
      }
    } else {
      rel(s, getIntVar(s, ce[1]), swap(irt), ce[0]->getInt(), 
          ann2icl(ann));
    }
  }
  void p_int_eq(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP(s, IRT_EQ, ce, ann);
  }
  void p_int_ne(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP(s, IRT_NQ, ce, ann);
  }
  void p_int_ge(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP(s, IRT_GQ, ce, ann);
  }
  void p_int_gt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP(s, IRT_GR, ce, ann);
  }
  void p_int_le(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP(s, IRT_LQ, ce, ann);
  }
  void p_int_lt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP(s, IRT_LE, ce, ann);
  }
  void p_int_CMP_reif(FlatZincGecode& s, IntRelType irt, const ConExpr& ce,
                      AST::Node* ann) {
    if (ce[2]->isBool()) {
      if (ce[2]->getBool()) {
        p_int_CMP(s, irt, ce, ann);
      } else {
        p_int_CMP(s, neg(irt), ce, ann);
      }
      return;
    }
    if (ce[0]->isIntVar()) {
      if (ce[1]->isIntVar()) {
        rel(s, getIntVar(s, ce[0]), irt, getIntVar(s, ce[1]),
               getBoolVar(s, ce[2]), ann2icl(ann));
      } else {
        rel(s, getIntVar(s, ce[0]), irt, ce[1]->getInt(),
               getBoolVar(s, ce[2]), ann2icl(ann));
      }
    } else {
      rel(s, getIntVar(s, ce[1]), swap(irt), ce[0]->getInt(),
             getBoolVar(s, ce[2]), ann2icl(ann));
    }
  }

  /* Comparisons */
  void p_int_eq_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP_reif(s, IRT_EQ, ce, ann);
  }
  void p_int_ne_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP_reif(s, IRT_NQ, ce, ann);
  }
  void p_int_ge_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP_reif(s, IRT_GQ, ce, ann);
  }
  void p_int_gt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP_reif(s, IRT_GR, ce, ann);
  }
  void p_int_le_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP_reif(s, IRT_LQ, ce, ann);
  }
  void p_int_lt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_CMP_reif(s, IRT_LE, ce, ann);
  }

  /* linear (in-)equations */
  void p_int_lin_CMP(FlatZincGecode& s, IntRelType irt, const ConExpr& ce,
                     AST::Node* ann) {
    IntArgs ia = arg2intargs(ce[0]);
    IntVarArgs iv = arg2intvarargs(s, ce[1]);
    linear(s, ia, iv, irt, ce[2]->getInt(), ann2icl(ann));
  }
  void p_int_lin_CMP_reif(FlatZincGecode& s, IntRelType irt,
                          const ConExpr& ce, AST::Node* ann) {
    if (ce[2]->isBool()) {
      if (ce[2]->getBool()) {
        p_int_lin_CMP(s, irt, ce, ann);
      } else {
        p_int_lin_CMP(s, neg(irt), ce, ann);
      }
      return;
    }
    IntArgs ia = arg2intargs(ce[0]);
    IntVarArgs iv = arg2intvarargs(s, ce[1]);
    linear(s, ia, iv, irt, ce[2]->getInt(), getBoolVar(s, ce[3]), 
           ann2icl(ann));
  }
  void p_int_lin_eq(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP(s, IRT_EQ, ce, ann);
  }
  void p_int_lin_eq_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP_reif(s, IRT_EQ, ce, ann);    
  }
  void p_int_lin_ne(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP(s, IRT_NQ, ce, ann);
  }
  void p_int_lin_ne_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP_reif(s, IRT_NQ, ce, ann);    
  }
  void p_int_lin_le(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP(s, IRT_LQ, ce, ann);
  }
  void p_int_lin_le_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP_reif(s, IRT_LQ, ce, ann);    
  }
  void p_int_lin_lt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP(s, IRT_LE, ce, ann);
  }
  void p_int_lin_lt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP_reif(s, IRT_LE, ce, ann);    
  }
  void p_int_lin_ge(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP(s, IRT_GQ, ce, ann);
  }
  void p_int_lin_ge_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP_reif(s, IRT_GQ, ce, ann);    
  }
  void p_int_lin_gt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP(s, IRT_GR, ce, ann);
  }
  void p_int_lin_gt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_int_lin_CMP_reif(s, IRT_GR, ce, ann);    
  }

  void p_bool_lin_CMP(FlatZincGecode& s, IntRelType irt, const ConExpr& ce,
                      AST::Node* ann) {
    IntArgs ia = arg2intargs(ce[0]);
    BoolVarArgs iv = arg2boolvarargs(s, ce[1]);
    if (ce[2]->isIntVar())
      linear(s, ia, iv, irt, s.iv[ce[2]->getIntVar()], ann2icl(ann));
    else
      linear(s, ia, iv, irt, ce[2]->getInt(), ann2icl(ann));
  }
  void p_bool_lin_CMP_reif(FlatZincGecode& s, IntRelType irt,
                          const ConExpr& ce, AST::Node* ann) {
    if (ce[2]->isBool()) {
      if (ce[2]->getBool()) {
        p_bool_lin_CMP(s, irt, ce, ann);
      } else {
        p_bool_lin_CMP(s, neg(irt), ce, ann);
      }
      return;
    }
    IntArgs ia = arg2intargs(ce[0]);
    BoolVarArgs iv = arg2boolvarargs(s, ce[1]);
    if (ce[2]->isIntVar())
      linear(s, ia, iv, irt, s.iv[ce[2]->getIntVar()], getBoolVar(s, ce[3]), 
             ann2icl(ann));
    else
      linear(s, ia, iv, irt, ce[2]->getInt(), getBoolVar(s, ce[3]), 
             ann2icl(ann));
  }
  void p_bool_lin_eq(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_lin_CMP(s, IRT_EQ, ce, ann);
  }
  void p_bool_lin_eq_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP_reif(s, IRT_EQ, ce, ann);
  }
  void p_bool_lin_ne(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_lin_CMP(s, IRT_NQ, ce, ann);
  }
  void p_bool_lin_ne_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP_reif(s, IRT_NQ, ce, ann);
  }
  void p_bool_lin_le(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_lin_CMP(s, IRT_LQ, ce, ann);
  }
  void p_bool_lin_le_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP_reif(s, IRT_LQ, ce, ann);
  }
  void p_bool_lin_lt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP(s, IRT_LE, ce, ann);
  }
  void p_bool_lin_lt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP_reif(s, IRT_LE, ce, ann);
  }
  void p_bool_lin_ge(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_lin_CMP(s, IRT_GQ, ce, ann);
  }
  void p_bool_lin_ge_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP_reif(s, IRT_GQ, ce, ann);
  }
  void p_bool_lin_gt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_lin_CMP(s, IRT_GR, ce, ann);
  }
  void p_bool_lin_gt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) 
  {
    p_bool_lin_CMP_reif(s, IRT_GR, ce, ann);
  }

  /* arithmetic constraints */
  
  void p_int_plus(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    if (!ce[0]->isIntVar()) {
      post(s, ce[0]->getInt() + getIntVar(s, ce[1])
              == getIntVar(s,ce[2]), ann2icl(ann));
    } else if (!ce[1]->isIntVar()) {
      post(s, getIntVar(s,ce[0]) + ce[1]->getInt()
              == getIntVar(s,ce[2]), ann2icl(ann));
    } else if (!ce[2]->isIntVar()) {
      post(s, getIntVar(s,ce[0]) + getIntVar(s,ce[1]) 
              == ce[2]->getInt(), ann2icl(ann));
    } else {
      post(s, getIntVar(s,ce[0]) + getIntVar(s,ce[1]) 
              == getIntVar(s,ce[2]), ann2icl(ann));
    }
  }

  void p_int_minus(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    if (!ce[0]->isIntVar()) {
      post(s, ce[0]->getInt() - getIntVar(s, ce[1])
              == getIntVar(s,ce[2]), ann2icl(ann));
    } else if (!ce[1]->isIntVar()) {
      post(s, getIntVar(s,ce[0]) - ce[1]->getInt()
              == getIntVar(s,ce[2]), ann2icl(ann));
    } else if (!ce[2]->isIntVar()) {
      post(s, getIntVar(s,ce[0]) - getIntVar(s,ce[1]) 
              == ce[2]->getInt(), ann2icl(ann));
    } else {
      post(s, getIntVar(s,ce[0]) - getIntVar(s,ce[1]) 
              == getIntVar(s,ce[2]), ann2icl(ann));
    }
  }

  void p_int_times(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    IntVar x2 = getIntVar(s, ce[2]);
    mult(s, x0, x1, x2, ann2icl(ann));    
  }
  void p_int_div(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    IntVar x2 = getIntVar(s, ce[2]);
    div(s,x0,x1,x2, ann2icl(ann));
  }
  void p_int_mod(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    IntVar x2 = getIntVar(s, ce[2]);
    mod(s,x0,x1,x2, ann2icl(ann));
  }

  void p_int_min(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    IntVar x2 = getIntVar(s, ce[2]);
    min(s, x0, x1, x2, ann2icl(ann));
  }
  void p_int_max(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    IntVar x2 = getIntVar(s, ce[2]);
    max(s, x0, x1, x2, ann2icl(ann));
  }
  void p_int_negate(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    post(s, x0 == -x1, ann2icl(ann));
  }

  /* Boolean constraints */
  void p_bool_CMP(FlatZincGecode& s, IntRelType irt, const ConExpr& ce, 
                 AST::Node* ann) {
    if (ce[0]->isBoolVar()) {
      if (ce[1]->isBoolVar()) {
        rel(s, getBoolVar(s, ce[0]), irt, getBoolVar(s, ce[1]), 
            ann2icl(ann));
      } else {
        rel(s, getBoolVar(s, ce[0]), irt, ce[1]->getBool(), ann2icl(ann));
      }
    } else {
      rel(s, getBoolVar(s, ce[1]), swap(irt), ce[0]->getBool(), 
          ann2icl(ann));
    }
  }
  void p_bool_CMP_reif(FlatZincGecode& s, IntRelType irt, const ConExpr& ce, 
                 AST::Node* ann) {
    rel(s, getBoolVar(s, ce[0]), irt, getBoolVar(s, ce[1]),
          getBoolVar(s, ce[2]), ann2icl(ann));
  }
  void p_bool_eq(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP(s, IRT_EQ, ce, ann);
  }
  void p_bool_eq_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP_reif(s, IRT_EQ, ce, ann);
  }
  void p_bool_ne(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP(s, IRT_NQ, ce, ann);
  }
  void p_bool_ne_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP_reif(s, IRT_NQ, ce, ann);
  }
  void p_bool_ge(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP(s, IRT_GQ, ce, ann);
  }
  void p_bool_ge_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP_reif(s, IRT_GQ, ce, ann);
  }
  void p_bool_le(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP(s, IRT_LQ, ce, ann);
  }
  void p_bool_le_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP_reif(s, IRT_LQ, ce, ann);
  }
  void p_bool_gt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP(s, IRT_GR, ce, ann);
  }
  void p_bool_gt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP_reif(s, IRT_GR, ce, ann);
  }
  void p_bool_lt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP(s, IRT_LE, ce, ann);
  }
  void p_bool_lt_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    p_bool_CMP_reif(s, IRT_LE, ce, ann);
  }

#define BOOL_OP(op) \
  BoolVar b0 = getBoolVar(s, ce[0]); \
  BoolVar b1 = getBoolVar(s, ce[1]); \
  if (ce[2]->isBool()) { \
    rel(s, b0, op, b1, ce[2]->getBool(), ann2icl(ann)); \
  } else { \
    rel(s, b0, op, b1, s.bv[ce[2]->getBoolVar()], ann2icl(ann)); \
  }

#define BOOL_ARRAY_OP(op) \
  BoolVarArgs bv = arg2boolvarargs(s, ce[0]); \
  if (ce[1]->isBool()) { \
    rel(s, op, bv, ce[1]->getBool(), ann2icl(ann)); \
  } else { \
    rel(s, op, bv, s.bv[ce[1]->getBoolVar()], ann2icl(ann)); \
  }

  void p_bool_or(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BOOL_OP(BOT_OR);
  }
  void p_bool_and(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BOOL_OP(BOT_AND);
  }
  void p_array_bool_and(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann)
  {
    BOOL_ARRAY_OP(BOT_AND);
  }
  void p_array_bool_or(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann)
  {
    BOOL_ARRAY_OP(BOT_OR);
  }
  void p_array_bool_clause(FlatZincGecode& s, const ConExpr& ce,
                           AST::Node* ann) {
    BoolVarArgs bvp = arg2boolvarargs(s, ce[0]);
    BoolVarArgs bvn = arg2boolvarargs(s, ce[1]);
    clause(s, BOT_OR, bvp, bvn, 1, ann2icl(ann));
  }
  void p_array_bool_clause_reif(FlatZincGecode& s, const ConExpr& ce,
                           AST::Node* ann) {
    BoolVarArgs bvp = arg2boolvarargs(s, ce[0]);
    BoolVarArgs bvn = arg2boolvarargs(s, ce[1]);
    BoolVar b0 = getBoolVar(s, ce[2]);
    clause(s, BOT_OR, bvp, bvn, b0, ann2icl(ann));
  }
  void p_bool_xor(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BOOL_OP(BOT_XOR);
  }
  void p_bool_l_imp(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BoolVar b0 = getBoolVar(s, ce[0]);
    BoolVar b1 = getBoolVar(s, ce[1]);
    if (ce[2]->isBool()) {
      rel(s, b1, BOT_IMP, b0, ce[2]->getBool(), ann2icl(ann));
    } else {
      rel(s, b1, BOT_IMP, b0, s.bv[ce[2]->getBoolVar()], ann2icl(ann));
    }
  }
  void p_bool_r_imp(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BOOL_OP(BOT_IMP);
  }
  void p_bool_not(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BoolVar x0 = getBoolVar(s, ce[0]);
    BoolVar x1 = getBoolVar(s, ce[1]);
    rel(s, x0, BOT_XOR, x1, 1, ann2icl(ann));
  }
  
  /* element constraints */
  void p_array_int_element(FlatZincGecode& s, const ConExpr& ce,
                           AST::Node* ann) {
    IntArgs ia = arg2intargs(ce[1], 1);
    IntVar selector = getIntVar(s, ce[0]);
    post(s, selector > 0);
    element(s, ia, selector,
                   getIntVar(s, ce[2]), ann2icl(ann));
  }
  void p_array_var_int_element(FlatZincGecode& s, const ConExpr& ce, 
                               AST::Node* ann) {
    IntVarArgs iv = arg2intvarargs(s, ce[1], 1);
    IntVar selector = getIntVar(s, ce[0]);
    post(s, selector > 0);
    element(s, iv, selector, getIntVar(s, ce[2]),
            ann2icl(ann));
  }
  void p_array_bool_element(FlatZincGecode& s, const ConExpr& ce, 
                                AST::Node* ann) {
    IntArgs bv = arg2boolargs(ce[1], 1);
    IntVar selector = getIntVar(s, ce[0]);
    post(s, selector > 0);
    element(s, bv, selector, getBoolVar(s, ce[2]),
            ann2icl(ann));
  }
  void p_array_var_bool_element(FlatZincGecode& s, const ConExpr& ce, 
                                AST::Node* ann) {
    BoolVarArgs bv = arg2boolvarargs(s, ce[1], 1);
    IntVar selector = getIntVar(s, ce[0]);
    post(s, selector > 0);
    element(s, bv, selector, getBoolVar(s, ce[2]),
            ann2icl(ann));
  }
  
  /* coercion constraints */
  void p_bool2int(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BoolVar x0 = getBoolVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    channel(s, x0, x1, ann2icl(ann));
  }

  /* constraints from the standard library */
  
  void p_abs(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVar x0 = getIntVar(s, ce[0]);
    IntVar x1 = getIntVar(s, ce[1]);
    abs(s, x0, x1, ann2icl(ann));
  }
  
  void p_array_int_lt(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv0 = arg2intvarargs(s, ce[0]);
    IntVarArgs iv1 = arg2intvarargs(s, ce[1]);
    rel(s, iv0, IRT_LE, iv1);
  }

  void p_array_int_lq(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv0 = arg2intvarargs(s, ce[0]);
    IntVarArgs iv1 = arg2intvarargs(s, ce[1]);
    rel(s, iv0, IRT_LQ, iv1);
  }
  
  void p_count(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv = arg2intvarargs(s, ce[0]);
    if (!ce[1]->isIntVar()) {
      if (!ce[2]->isIntVar()) {
        count(s, iv, ce[1]->getInt(), IRT_EQ, ce[2]->getInt(), 
              ann2icl(ann));
      } else {
        count(s, iv, ce[1]->getInt(), IRT_EQ, getIntVar(s, ce[2]), 
              ann2icl(ann));
      }
    } else if (!ce[2]->isIntVar()) {
      count(s, iv, getIntVar(s, ce[1]), IRT_EQ, ce[2]->getInt(), 
            ann2icl(ann));
    } else {
      count(s, iv, getIntVar(s, ce[1]), IRT_EQ, getIntVar(s, ce[2]), 
            ann2icl(ann));
    }
  }

  void count_rel(IntRelType irt,
                 FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv = arg2intvarargs(s, ce[1]);
    count(s, iv, ce[2]->getInt(), irt, ce[0]->getInt(), ann2icl(ann));
  }

  void p_at_most(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    count_rel(IRT_LQ, s, ce, ann);
  }

  void p_at_least(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    count_rel(IRT_GQ, s, ce, ann);
  }

  void p_global_cardinality(FlatZincGecode& s, const ConExpr& ce,
                            AST::Node* ann) {
    IntVarArgs iv0 = arg2intvarargs(s, ce[0]);
    IntVarArgs iv1 = arg2intvarargs(s, ce[1]);
    int cmin = ce[2]->getInt();
    if (cmin == 0) {
      count(s, iv0, iv1, ann2icl(ann));      
    } else {
      IntArgs values(iv1.size());
      for (int i=values.size(); i--;)
        values[i] = i+cmin;
      count(s, iv0, iv1, values, ann2icl(ann));
    }
  }

  void p_minimum(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv = arg2intvarargs(s, ce[1]);
    min(s, iv, getIntVar(s, ce[0]));
  }

  void p_maximum(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv = arg2intvarargs(s, ce[1]);
    max(s, iv, getIntVar(s, ce[0]));
  }

  void p_regular(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs iv = arg2intvarargs(s, ce[0]);
    int q = ce[1]->getInt();
    int symbols = ce[2]->getInt();
    IntArgs d = arg2intargs(ce[3]);
    int q0 = ce[4]->getInt();

    int noOfTrans = 0;
    for (int i=1; i<=q; i++) {
      for (int j=1; j<=symbols; j++) {
        if (d[(i-1)*symbols+(j-1)] > 0)
          noOfTrans++;
      }
    }
    
    Region re(s);
    DFA::Transition* t = re.alloc<DFA::Transition>(noOfTrans+1);
    noOfTrans = 0;
    for (int i=1; i<=q; i++) {
      for (int j=1; j<=symbols; j++) {
        if (d[(i-1)*symbols+(j-1)] > 0) {
          t[noOfTrans].i_state = i;
          t[noOfTrans].symbol  = j;
          t[noOfTrans].o_state = d[(i-1)*symbols+(j-1)];
          noOfTrans++;
        }
      }
    }
    t[noOfTrans].i_state = -1;
    
    // Final states
    AST::SetLit* sl = ce[5]->getSet();
    int* f;
    if (sl->interval) {
      f = static_cast<int*>(malloc(sizeof(int)*(sl->max-sl->min+2)));
      for (int i=sl->min; i<=sl->max; i++)
        f[i-sl->min] = i;
      f[sl->max-sl->min+1] = -1;
    } else {
      f = static_cast<int*>(malloc(sizeof(int)*(sl->s.size()+1)));
      for (int j=sl->s.size(); j--; )
        f[j] = sl->s[j];
      f[sl->s.size()] = -1;
    }
        
    DFA dfa(q0,t,f);
    free(f);
    extensional(s, iv, dfa, ann2icl(ann));
  }

  void
  p_sort(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs x = arg2intvarargs(s, ce[0]);
    IntVarArgs y = arg2intvarargs(s, ce[1]);
    IntVarArgs xy(x.size()+y.size());
    for (int i=x.size(); i--;)
      xy[i] = x[i];
    for (int i=y.size(); i--;)
      xy[i+x.size()] = y[i];
    unshare(s, xy);
    for (int i=x.size(); i--;)
      x[i] = xy[i];
    for (int i=y.size(); i--;)
      y[i] = xy[i+x.size()];
    sorted(s, x, y, ann2icl(ann));
  }

  void
  p_inverse_offsets(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs x = arg2intvarargs(s, ce[0]);
    int xoff = ce[1]->getInt();
    IntVarArgs y = arg2intvarargs(s, ce[2]);
    int yoff = ce[3]->getInt();
    channel(s, x, xoff, y, yoff, ann2icl(ann));
  }

  void
  p_increasing_int(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs x = arg2intvarargs(s, ce[0]);
    rel(s,x,IRT_LQ,ann2icl(ann));
  }

  void
  p_increasing_bool(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BoolVarArgs x = arg2boolvarargs(s, ce[0]);
    rel(s,x,IRT_LQ,ann2icl(ann));
  }

  void
  p_table_int(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    IntVarArgs x = arg2intvarargs(s, ce[0]);
    IntArgs tuples = arg2intargs(ce[1]);
    int noOfVars   = x.size();
    int noOfTuples = tuples.size()/noOfVars;
    TupleSet ts;
    for (int i=0; i<noOfTuples; i++) {
      IntArgs t(noOfVars);
      for (int j=0; j<x.size(); j++) {
        t[j] = tuples[i*noOfVars+j];
      }
      ts.add(t);
    }
    ts.finalize();
    extensional(s,x,ts,EPK_DEF,ann2icl(ann));
  }
  void
  p_table_bool(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    BoolVarArgs x = arg2boolvarargs(s, ce[0]);
    IntArgs tuples = arg2boolargs(ce[1]);
    int noOfVars   = x.size();
    int noOfTuples = tuples.size()/noOfVars;
    TupleSet ts;
    for (int i=0; i<noOfTuples; i++) {
      IntArgs t(noOfVars);
      for (int j=0; j<x.size(); j++) {
        t[j] = tuples[i*noOfVars+j];
      }
      ts.add(t);
    }
    ts.finalize();
    extensional(s,x,ts,EPK_DEF,ann2icl(ann));
  }

  void p_cumulatives(FlatZincGecode& s, const ConExpr& ce,
                    AST::Node* ann) {
    IntVarArgs start = arg2intvarargs(s, ce[0]);
    IntVarArgs duration = arg2intvarargs(s, ce[1]);
    IntVarArgs height = arg2intvarargs(s, ce[2]);
    int n = start.size();
    IntVar bound = getIntVar(s, ce[3]);

    if (bound.assigned()) {
      IntArgs machine(n);
      for (int i = n; i--; ) machine[i] = 0;
      IntArgs limit(1, bound.val());
      IntVarArgs end(n);
      for (int i = n; i--; ) end[i] = IntVar(s, 0, Int::Limits::max);
      cumulatives(s, machine, start, duration, end, height, limit, true);
    } else {
      int min = Gecode::Int::Limits::max;
      int max = Gecode::Int::Limits::min;
      IntVarArgs end(start.size());
      for (int i = start.size(); i--; ) {
        min = std::min(min, start[i].min());
        max = std::max(max, start[i].max() + duration[i].max());
        end[i] = post(s, start[i] + duration[i]);
      }
      for (int time = min; time < max; ++time) {
        IntVarArgs x(start.size());
        for (int i = start.size(); i--; ) {
          IntVar overlaps = channel(s, post(s, (~(start[i] <= time) && 
                                                ~(time < end[i]))));
          x[i] = mult(s, overlaps, height[i]);
        }
        linear(s, x, IRT_LQ, bound);
      }
    }
  }

  class IntPoster {
  public:
    IntPoster(void) {
      registry.add("all_different_int", &p_distinct);
      registry.add("all_different_offset", &p_distinctOffset);
      registry.add("int_eq", &p_int_eq);
      registry.add("int_ne", &p_int_ne);
      registry.add("int_ge", &p_int_ge);
      registry.add("int_gt", &p_int_gt);
      registry.add("int_le", &p_int_le);
      registry.add("int_lt", &p_int_lt);
      registry.add("int_eq_reif", &p_int_eq_reif);
      registry.add("int_ne_reif", &p_int_ne_reif);
      registry.add("int_ge_reif", &p_int_ge_reif);
      registry.add("int_gt_reif", &p_int_gt_reif);
      registry.add("int_le_reif", &p_int_le_reif);
      registry.add("int_lt_reif", &p_int_lt_reif);
      registry.add("int_lin_eq", &p_int_lin_eq);
      registry.add("int_lin_eq_reif", &p_int_lin_eq_reif);
      registry.add("int_lin_ne", &p_int_lin_ne);
      registry.add("int_lin_ne_reif", &p_int_lin_ne_reif);
      registry.add("int_lin_le", &p_int_lin_le);
      registry.add("int_lin_le_reif", &p_int_lin_le_reif);
      registry.add("int_lin_lt", &p_int_lin_lt);
      registry.add("int_lin_lt_reif", &p_int_lin_lt_reif);
      registry.add("int_lin_ge", &p_int_lin_ge);
      registry.add("int_lin_ge_reif", &p_int_lin_ge_reif);
      registry.add("int_lin_gt", &p_int_lin_gt);
      registry.add("int_lin_gt_reif", &p_int_lin_gt_reif);
      registry.add("int_plus", &p_int_plus);
      registry.add("int_minus", &p_int_minus);
      registry.add("int_times", &p_int_times);
      registry.add("int_div", &p_int_div);
      registry.add("int_mod", &p_int_mod);
      registry.add("int_min", &p_int_min);
      registry.add("int_max", &p_int_max);
      registry.add("int_abs", &p_abs);
      registry.add("int_negate", &p_int_negate);
      registry.add("bool_eq", &p_bool_eq);
      registry.add("bool_eq_reif", &p_bool_eq_reif);
      registry.add("bool_ne", &p_bool_ne);
      registry.add("bool_ne_reif", &p_bool_ne_reif);
      registry.add("bool_ge", &p_bool_ge);
      registry.add("bool_ge_reif", &p_bool_ge_reif);
      registry.add("bool_le", &p_bool_le);
      registry.add("bool_le_reif", &p_bool_le_reif);
      registry.add("bool_gt", &p_bool_gt);
      registry.add("bool_gt_reif", &p_bool_gt_reif);
      registry.add("bool_lt", &p_bool_lt);
      registry.add("bool_lt_reif", &p_bool_lt_reif);
      registry.add("bool_or", &p_bool_or);
      registry.add("bool_and", &p_bool_and);
      registry.add("bool_xor", &p_bool_xor);
      registry.add("array_bool_and", &p_array_bool_and);
      registry.add("array_bool_or", &p_array_bool_or);
      registry.add("bool_clause", &p_array_bool_clause);
      registry.add("bool_clause_reif", &p_array_bool_clause_reif);
      registry.add("bool_left_imp", &p_bool_l_imp);
      registry.add("bool_right_imp", &p_bool_r_imp);
      registry.add("bool_not", &p_bool_not);
      registry.add("array_int_element", &p_array_int_element);
      registry.add("array_var_int_element", &p_array_var_int_element);
      registry.add("array_bool_element", &p_array_bool_element);
      registry.add("array_var_bool_element", &p_array_var_bool_element);
      registry.add("bool2int", &p_bool2int);
      
      registry.add("array_int_lt", &p_array_int_lt);
      registry.add("array_int_lq", &p_array_int_lq);
      registry.add("count", &p_count);
      registry.add("at_least_int", &p_at_least);      
      registry.add("at_most_int", &p_at_most);
      registry.add("global_cardinality_gecode", &p_global_cardinality);
      registry.add("minimum_int", &p_minimum);
      registry.add("maximum_int", &p_maximum);
      registry.add("regular", &p_regular);
      registry.add("sort", &p_sort);
      registry.add("inverse_offsets", &p_inverse_offsets);
      registry.add("increasing_int", &p_increasing_int);
      registry.add("increasing_bool", &p_increasing_bool);
      registry.add("table_int", &p_table_int);
      registry.add("table_bool", &p_table_bool);
      registry.add("cumulatives", &p_cumulatives);

      registry.add("bool_lin_eq", &p_bool_lin_eq);
      registry.add("bool_lin_ne", &p_bool_lin_ne);
      registry.add("bool_lin_le", &p_bool_lin_le);
      registry.add("bool_lin_lt", &p_bool_lin_lt);
      registry.add("bool_lin_ge", &p_bool_lin_ge);
      registry.add("bool_lin_gt", &p_bool_lin_gt);

      registry.add("bool_lin_eq_reif", &p_bool_lin_eq_reif);
      registry.add("bool_lin_ne_reif", &p_bool_lin_ne_reif);
      registry.add("bool_lin_le_reif", &p_bool_lin_le_reif);
      registry.add("bool_lin_lt_reif", &p_bool_lin_lt_reif);
      registry.add("bool_lin_ge_reif", &p_bool_lin_ge_reif);
      registry.add("bool_lin_gt_reif", &p_bool_lin_gt_reif);
    }
  };
  IntPoster __int_poster;

#ifdef GECODE_HAS_SET_VARS
  void p_set_OP(FlatZincGecode& s, SetOpType op,
                const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), op, getSetVar(s, ce[1]), 
        SRT_EQ, getSetVar(s, ce[2]));
  }
  void p_set_union(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    p_set_OP(s, SOT_UNION, ce, ann);
  }
  void p_set_intersect(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    p_set_OP(s, SOT_INTER, ce, ann);
  }
  void p_set_diff(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    p_set_OP(s, SOT_MINUS, ce, ann);
  }

  void p_set_symdiff(FlatZincGecode& s, const ConExpr& ce, AST::Node* ann) {
    SetVar x = getSetVar(s, ce[0]);
    SetVar y = getSetVar(s, ce[1]);

    SetVarLubRanges xub(x);
    IntSet xubs(xub);
    SetVar x_y(s,IntSet::empty,xubs);
    rel(s, x, SOT_MINUS, y, SRT_EQ, x_y);

    SetVarLubRanges yub(y);
    IntSet yubs(yub);
    SetVar y_x(s,IntSet::empty,yubs);
    rel(s, y, SOT_MINUS, x, SRT_EQ, y_x);
    
    rel(s, x_y, SOT_UNION, y_x, SRT_EQ, getSetVar(s, ce[2]));
  }

  void p_array_set_OP(FlatZincGecode& s, SetOpType op,
                      const ConExpr& ce, AST::Node *ann) {
    SetVarArgs xs = arg2setvarargs(s, ce[0]);
    rel(s, op, xs, getSetVar(s, ce[1]));
  }
  void p_array_set_union(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    p_array_set_OP(s, SOT_UNION, ce, ann);
  }
  void p_array_set_partition(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    p_array_set_OP(s, SOT_DUNION, ce, ann);
  }


  void p_set_eq(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_EQ, getSetVar(s, ce[1]));
  }
  void p_set_ne(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_NQ, getSetVar(s, ce[1]));
  }
  void p_set_subset(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_SUB, getSetVar(s, ce[1]));
  }
  void p_set_superset(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_SUP, getSetVar(s, ce[1]));
  }
  void p_set_card(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    if (!ce[1]->isIntVar()) {
      cardinality(s, getSetVar(s, ce[0]), ce[1]->getInt(), 
                  ce[1]->getInt());
    } else {
      cardinality(s, getSetVar(s, ce[0]), getIntVar(s, ce[1]));
    }
  }
  void p_set_in(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    if (!ce[1]->isSetVar()) {
      AST::SetLit* sl = ce[1]->getSet();
      IntSet d;
      if (sl->interval) {
        d = IntSet(sl->min, sl->max);
      } else {
        Region re(s);
        int* is = re.alloc<int>(static_cast<unsigned long int>(sl->s.size()));
        for (int i=sl->s.size(); i--; )
          is[i] = sl->s[i];
        d = IntSet(is, sl->s.size());
      }
      if (ce[0]->isBoolVar()) {
        assert(sl->interval);
        rel(s, getBoolVar(s, ce[0]), IRT_GQ, sl->min);
        rel(s, getBoolVar(s, ce[0]), IRT_LQ, sl->max);
      } else {
        dom(s, getIntVar(s, ce[0]), d);
      }
    } else {
      if (!ce[0]->isIntVar()) {
        dom(s, getSetVar(s, ce[1]), SRT_SUP, ce[0]->getInt());
      } else {
        rel(s, getSetVar(s, ce[1]), SRT_SUP, getIntVar(s, ce[0]));
      }
    }
  }
  void p_set_eq_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_EQ, getSetVar(s, ce[1]),
        getBoolVar(s, ce[2]));
  }
  void p_set_ne_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_NQ, getSetVar(s, ce[1]),
        getBoolVar(s, ce[2]));
  }
  void p_set_subset_reif(FlatZincGecode& s, const ConExpr& ce,
                         AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_SUB, getSetVar(s, ce[1]),
        getBoolVar(s, ce[2]));
  }
  void p_set_superset_reif(FlatZincGecode& s, const ConExpr& ce,
                           AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_SUP, getSetVar(s, ce[1]),
        getBoolVar(s, ce[2]));
  }
  void p_set_in_reif(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    if (!ce[1]->isSetVar()) {
      AST::SetLit* sl = ce[1]->getSet();
      IntSet d;
      if (sl->interval) {
        d = IntSet(sl->min, sl->max);
      } else {
        Region re(s);
        int* is = re.alloc<int>(static_cast<unsigned long int>(sl->s.size()));
        for (int i=sl->s.size(); i--; )
          is[i] = sl->s[i];
        d = IntSet(is, sl->s.size());
      }
      if (ce[0]->isBoolVar()) {
        assert(sl->interval);
        rel(s, getBoolVar(s, ce[0]), IRT_GQ, sl->min, getBoolVar(s, ce[2]));
        rel(s, getBoolVar(s, ce[0]), IRT_LQ, sl->max, getBoolVar(s, ce[2]));
      } else {
        dom(s, getIntVar(s, ce[0]), d, getBoolVar(s, ce[2]));
      }
    } else {
      if (!ce[0]->isIntVar()) {
        dom(s, getSetVar(s, ce[1]), SRT_SUP, ce[0]->getInt(),
            getBoolVar(s, ce[2]));
      } else {
        rel(s, getSetVar(s, ce[1]), SRT_SUP, getIntVar(s, ce[0]),
            getBoolVar(s, ce[2]));
      }
    }
  }
  void p_set_disjoint(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    rel(s, getSetVar(s, ce[0]), SRT_DISJ, getSetVar(s, ce[1]));
  }

  void p_array_set_element(FlatZincGecode& s, const ConExpr& ce,
                           AST::Node*) {
    IntSetArgs sv = arg2intsetargs(ce[1],1);
    IntVar selector = getIntVar(s, ce[0]);
    post(s, selector > 0);
    element(s, sv, selector, getSetVar(s, ce[2]));
  }
  void p_array_var_set_element(FlatZincGecode& s, const ConExpr& ce,
                               AST::Node* ann) {
    bool isConstant = true;
    AST::Array* a = ce[1]->getArray();
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isSetVar()) {
        isConstant = false;
        break;
      }
    }
    if (isConstant)
      return p_array_set_element(s,ce,ann);
    SetVarArgs sv = arg2setvarargs(s, ce[1], 1);
    IntVar selector = getIntVar(s, ce[0]);
    post(s, selector > 0);
    element(s, sv, selector, getSetVar(s, ce[2]));
  }

  void p_array_set_element_union(FlatZincGecode& s, const ConExpr& ce,
                                 AST::Node*) {
    IntSetArgs sv = arg2intsetargs(ce[1], 1);
    SetVar selector = getSetVar(s, ce[0]);
    dom(s, selector, SRT_DISJ, 0);
    element(s, SOT_UNION, sv, selector, getSetVar(s, ce[2]));
  }

  void p_array_var_set_element_union(FlatZincGecode& s, const ConExpr& ce,
                                     AST::Node* ann) {
    bool isConstant = true;
    AST::Array* a = ce[1]->getArray();
    for (int i=a->a.size(); i--;) {
      if (a->a[i]->isSetVar()) {
        isConstant = false;
        break;
      }
    }
    if (isConstant)
      return p_array_set_element_union(s,ce,ann);
    SetVarArgs sv = arg2setvarargs(s, ce[1], 1);
    SetVar selector = getSetVar(s, ce[0]);
    dom(s, selector, SRT_DISJ, 0);
    element(s, SOT_UNION, sv, selector, getSetVar(s, ce[2]));
  }

  void p_set_convex(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    convex(s, getSetVar(s, ce[0]));
  }

  void p_array_set_seq(FlatZincGecode& s, const ConExpr& ce, AST::Node *ann) {
    SetVarArgs sv = arg2setvarargs(s, ce[0]);
    sequence(s, sv);
  }

  void p_array_set_seq_union(FlatZincGecode& s, const ConExpr& ce,
                             AST::Node *ann) {
    SetVarArgs sv = arg2setvarargs(s, ce[0]);
    sequence(s, sv, getSetVar(s, ce[1]));
  }

  class SetPoster {
  public:
    SetPoster(void) {
      registry.add("set_eq", &p_set_eq);
      registry.add("equal", &p_set_eq);
      registry.add("set_ne", &p_set_ne);
      registry.add("set_union", &p_set_union);
      registry.add("array_set_element", &p_array_set_element);
      registry.add("array_var_set_element", &p_array_var_set_element);
      registry.add("set_intersect", &p_set_intersect);
      registry.add("set_diff", &p_set_diff);
      registry.add("set_symdiff", &p_set_symdiff);
      registry.add("set_subset", &p_set_subset);
      registry.add("set_superset", &p_set_superset);
      registry.add("set_card", &p_set_card);
      registry.add("set_in", &p_set_in);
      registry.add("set_eq_reif", &p_set_eq_reif);
      registry.add("equal_reif", &p_set_eq_reif);
      registry.add("set_ne_reif", &p_set_ne_reif);
      registry.add("set_subset_reif", &p_set_subset_reif);
      registry.add("set_superset_reif", &p_set_superset_reif);
      registry.add("set_in_reif", &p_set_in_reif);
      registry.add("disjoint", &p_set_disjoint);

      registry.add("array_set_union", &p_array_set_union);
      registry.add("array_set_partition", &p_array_set_partition);
      registry.add("set_convex", &p_set_convex);
      registry.add("array_set_seq", &p_array_set_seq);
      registry.add("array_set_seq_union", &p_array_set_seq_union);
      registry.add("array_set_element_union", &p_array_set_element_union);
      registry.add("array_var_set_element_union", 
                   &p_array_var_set_element_union);
    }
  };
  SetPoster __set_poster;
#endif

}