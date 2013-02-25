/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christopher Mears <chris.mears@monash.edu>
 *
 *  Copyright:
 *     Christopher Mears, 2012
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
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

#include <gecode/int/branch.hh>

namespace Gecode {
  /*
   * Implementation of some SymmetryHandle methods.
   */
  
  void
  SymmetryHandle::increment(void) {
    (ref->nrefs)++;
  }
  void
  SymmetryHandle::decrement(void) {
    (ref->nrefs)--;
    if (ref->nrefs == 0)
      delete ref;
    ref = NULL;
  }

  SymmetryHandle VariableSymmetry(IntVarArgs vars) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new Int::Branch::VariableSymmetryObject(a));
  }
  SymmetryHandle VariableSymmetry(BoolVarArgs vars) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new Int::Branch::VariableSymmetryObject(a));
  }
  SymmetryHandle ValueSymmetry(IntArgs vs) {
    return SymmetryHandle(new Int::Branch::ValueSymmetryObject(IntSet(vs)));
  }
  SymmetryHandle ValueSymmetry(IntSet vs) {
    return SymmetryHandle(new Int::Branch::ValueSymmetryObject(vs));
  }
  SymmetryHandle VariableSequenceSymmetry(IntVarArgs vars, int ss) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new Int::Branch::VariableSequenceSymmetryObject(a, ss));
  }
  SymmetryHandle VariableSequenceSymmetry(BoolVarArgs vars, int ss) {
    ArgArray<VarImpBase*> a(vars.size());
    for (int i = 0 ; i < vars.size() ; i++)
      a[i] = vars[i].varimp();
    return SymmetryHandle(new Int::Branch::VariableSequenceSymmetryObject(a, ss));
  }
  SymmetryHandle ValueSequenceSymmetry(IntArgs vs, int ss) {
    return SymmetryHandle(new Int::Branch::ValueSequenceSymmetryObject(vs, ss));
  }

  namespace Int { namespace Branch {
    /*
     * The duplication in createIntSym/createBoolSym is undesirable,
     * and so is the use of dynamic_cast to tell the symmetries
     * apart.
     */
    SymmetryImp<IntView>* createIntSym(Space& home, const SymmetryHandle& s,
                                       Branch::VariableMap variableMap) {
      VariableSymmetryObject*         varref    = dynamic_cast<VariableSymmetryObject*>(s.ref);
      ValueSymmetryObject*            valref    = dynamic_cast<ValueSymmetryObject*>(s.ref);
      VariableSequenceSymmetryObject* varseqref = dynamic_cast<VariableSequenceSymmetryObject*>(s.ref);
      ValueSequenceSymmetryObject*    valseqref = dynamic_cast<ValueSequenceSymmetryObject*>(s.ref);
      if (varref) {
        int n = varref->nxs;
        int* indices = home.alloc<int>(n);
        for (int i = 0 ; i < n ; i++) {
          VariableMap::const_iterator index = variableMap.find(varref->xs[i]);
          if (index == variableMap.end())
            throw LDSBBranchingException("VariableSymmetryObject::createInt");
          indices[i] = index->second;
        }
        return new (home) VariableSymmetryImp<IntView>(home, indices, n);
      }
      if (valref) {
        int n = valref->values.size();
        int *vs = home.alloc<int>(n);
        int i = 0;
        for (IntSetValues v(valref->values) ; v() ; ++v) {
          vs[i] = v.val();
          i++;
        }
        return new (home) ValueSymmetryImp<IntView>(home, vs, n);
      }
      if (varseqref) {
        int n = varseqref->nxs;
        int* indices = home.alloc<int>(n);
        for (int i = 0 ; i < n ; i++) {
          VariableMap::const_iterator index = variableMap.find(varseqref->xs[i]);
          if (index == variableMap.end())
            throw LDSBBranchingException("VariableSequenceSymmetryObject::createInt");
          indices[i] = index->second;
        }
        return new (home) VariableSequenceSymmetryImp<IntView>(home, indices, n, varseqref->seq_size);
      }
      if (valseqref) {
        unsigned int n = valseqref->values.size();
        int *vs = home.alloc<int>(n);
        for (unsigned int i = 0 ; i < n ; i++)
          vs[i] = valseqref->values[i];
        return new (home) ValueSequenceSymmetryImp<IntView>(home, vs, n, valseqref->seq_size);
      }
      std::cerr << "Unknown symmetry type in createIntSym." << std::endl;
      abort();
    }

      SymmetryImp<BoolView>* createBoolSym(Space& home, const SymmetryHandle& s,
                                           Branch::VariableMap variableMap) {
      VariableSymmetryObject*         varref    = dynamic_cast<VariableSymmetryObject*>(s.ref);
      ValueSymmetryObject*            valref    = dynamic_cast<ValueSymmetryObject*>(s.ref);
      VariableSequenceSymmetryObject* varseqref = dynamic_cast<VariableSequenceSymmetryObject*>(s.ref);
      ValueSequenceSymmetryObject*    valseqref = dynamic_cast<ValueSequenceSymmetryObject*>(s.ref);
      if (varref) {
        int n = varref->nxs;
        int* indices = home.alloc<int>(n);
        for (int i = 0 ; i < n ; i++) {
          VariableMap::const_iterator index = variableMap.find(varref->xs[i]);
          if (index == variableMap.end())
            throw LDSBBranchingException("VariableSymmetryObject::createBool");
          indices[i] = index->second;
        }
        return new (home) VariableSymmetryImp<BoolView>(home, indices, n);
      }
      if (valref) {
        int n = valref->values.size();
        int *vs = home.alloc<int>(n);
        int i = 0;
        for (IntSetValues v(valref->values) ; v() ; ++v) {
          vs[i] = v.val();
          i++;
        }
        return new (home) ValueSymmetryImp<BoolView>(home, vs, n);
      }
      if (varseqref) {
        int n = varseqref->nxs;
        int* indices = home.alloc<int>(n);
        for (int i = 0 ; i < n ; i++) {
          VariableMap::const_iterator index = variableMap.find(varseqref->xs[i]);
          if (index == variableMap.end())
            throw LDSBBranchingException("VariableSequenceSymmetryObject::createBool");
          indices[i] = index->second;
        }
        return new (home) VariableSequenceSymmetryImp<BoolView>(home, indices, n, varseqref->seq_size);
      }
      if (valseqref) {
        unsigned int n = valseqref->values.size();
        int *vs = home.alloc<int>(n);
        for (unsigned int i = 0 ; i < n ; i++)
          vs[i] = valseqref->values[i];
        return new (home) ValueSequenceSymmetryImp<BoolView>(home, vs, n, valseqref->seq_size);
      }
      std::cerr << "Unknown symmetry type in createBoolSym." << std::endl;
      abort();
    }
  }}

  template <>
  ArgArray<Literal>
  VariableSymmetryImp<Int::IntView>
  ::symmetric(Literal l, const ViewArray<Int::IntView>& x) const {
    (void) x;
    if (indices.valid(l.variable) && indices.get(l.variable)) {
      int n = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(indices) ; i() ; ++i)
        n++;
      ArgArray<Literal> lits(n);
      int j = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(indices) ; i() ; ++i)
        lits[j++] = Literal(i.val(), l.value);
      return lits;
    } else {
      return ArgArray<Literal>(0);
    }
  }
  template <>
  ArgArray<Literal>
  VariableSymmetryImp<Int::BoolView>
  ::symmetric(Literal l, const ViewArray<Int::BoolView>& x) const {
    (void) x;
    if (indices.valid(l.variable) && indices.get(l.variable)) {
      int n = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(indices) ; i() ; ++i)
        n++;
      ArgArray<Literal> lits(n);
      int j = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(indices) ; i() ; ++i)
        lits[j++] = Literal(i.val(), l.value);
      return lits;
    } else {
      return ArgArray<Literal>(0);
    }
  }

  template <>
  ArgArray<Literal>
  ValueSymmetryImp<Int::IntView>
  ::symmetric(Literal l, const ViewArray<Int::IntView>& x) const {
    (void) x;
    if (values.valid(l.value) && values.get(l.value)) {
      int n = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(values) ; i() ; ++i)
        n++;
      ArgArray<Literal> lits(n);
      int j = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(values) ; i() ; ++i)
        lits[j++] = Literal(l.variable, i.val());
      return lits;
    } else {
      return ArgArray<Literal>(0);
    }
  }
  template <>
  ArgArray<Literal>
  ValueSymmetryImp<Int::BoolView>
  ::symmetric(Literal l, const ViewArray<Int::BoolView>& x) const {
    (void) x;
    if (values.valid(l.value) && values.get(l.value)) {
      int n = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(values) ; i() ; ++i)
        n++;
      ArgArray<Literal> lits(n);
      int j = 0;
      for (Iter::Values::BitSetOffset<Support::BitSetOffset<Space> > i(values) ; i() ; ++i)
        lits[j++] = Literal(l.variable, i.val());
      return lits;
    } else {
      return ArgArray<Literal>(0);
    }
  }
  

  template <class View>
  ArgArray<Literal>
  VariableSequenceSymmetryImp<View>
  ::symmetric(Literal l, const ViewArray<View>& x) const {
    Support::DynamicStack<Literal, Heap> s(heap);
    if (l.variable < (int)lookup_size) {
      int posIt = lookup[l.variable];
      if (posIt == -1) {
        return Int::Branch::dynamicStackToArgArray(s);
      }
      unsigned int seqNum = posIt / seq_size;
      unsigned int seqPos = posIt % seq_size;
      for (unsigned int seq = 0 ; seq < n_seqs ; seq++) {
        if (seq == seqNum) {
          continue;
        }
        if (x[getVal(seq, seqPos)].assigned()) {
          continue;
        }
        bool active = true;
        const unsigned int *firstSeq = &indices[seqNum*seq_size];
        const unsigned int *secondSeq = &indices[seq*seq_size];
        for (unsigned int i = 0 ; i < seq_size ; i++) {
          const View& xv = x[firstSeq[i]];
          const View& yv = x[secondSeq[i]];
          if ((!xv.assigned() && !yv.assigned())
              || (xv.assigned() && yv.assigned() && xv.val() == yv.val())) {
            continue;
          } else {
            active = false;
            break;
          }
        }
        
        if (active) {
          s.push(Literal(secondSeq[seqPos], l.value));
        }
      }
    }
    return Int::Branch::dynamicStackToArgArray(s);
  }

  template <>
  ArgArray<Literal>
  ValueSequenceSymmetryImp<Int::IntView>
  ::symmetric(Literal l, const ViewArray<Int::IntView>& x) const {
    (void) x;
    Support::DynamicStack<Literal, Heap> s(heap);
    std::pair<int,int> location = Int::Branch::findVar(values, n_values, seq_size, l.value);
    if (location.first == -1) return Int::Branch::dynamicStackToArgArray(s);
    unsigned int seqNum = location.first;
    unsigned int seqPos = location.second;
    for (unsigned int seq = 0 ; seq < n_seqs ; seq++) {
      if (seq == seqNum) continue;
      if (dead_sequences.get(seq)) continue;
      s.push(Literal(l.variable, getVal(seq,seqPos)));
    }
    return Int::Branch::dynamicStackToArgArray(s);
  }
  template <>
  ArgArray<Literal>
  ValueSequenceSymmetryImp<Int::BoolView>
  ::symmetric(Literal l, const ViewArray<Int::BoolView>& x) const {
    (void) x;
    Support::DynamicStack<Literal, Heap> s(heap);
    std::pair<int,int> location = Int::Branch::findVar(values, n_values, seq_size, l.value);
    if (location.first == -1) return Int::Branch::dynamicStackToArgArray(s);
    unsigned int seqNum = location.first;
    unsigned int seqPos = location.second;
    for (unsigned int seq = 0 ; seq < n_seqs ; seq++) {
      if (seq == seqNum) continue;
      if (dead_sequences.get(seq)) continue;
      s.push(Literal(l.variable, getVal(seq,seqPos)));
    }
    return Int::Branch::dynamicStackToArgArray(s);
  }


  /*********************/

  SymmetryHandle
  variables_interchange(const IntVarArgs& vars) {
    return VariableSymmetry(vars);
  }

  SymmetryHandle
  variables_interchange(const BoolVarArgs& vars) {
    return VariableSymmetry(vars);
  }

  SymmetryHandle
  variables_indices_interchange(const IntVarArgs& vars, IntArgs indices) {
    IntVarArgs xs(indices.size());
    for (int i = 0 ; i < indices.size() ; i++)
      xs[i] = vars[indices[i]];
    return VariableSymmetry(xs);
  }

  SymmetryHandle
  values_interchange(const IntVarArgs& vars) {
    int min = vars[0].min();
    int max = vars[0].max();
    for (int i = 1 ; i < vars.size() ; i++) {
      min = std::min(min, vars[i].min());
      max = std::max(max, vars[i].max());
    }
    return ValueSymmetry(IntSet(min, max));
  }
}

namespace Gecode { namespace Int { namespace Branch {

  std::pair<int,int>
  findVar(int *indices, unsigned int n_values, unsigned int seq_size, int index)
  {
    unsigned int seq = 0;
    unsigned int pos = 0;
    for (unsigned int i = 0 ; i < n_values ; i++) {
      if (indices[i] == index)
        return std::pair<int,int>(seq,pos);
      pos++;
      if (pos == seq_size) {
        pos = 0;
        seq++;
      }
    }
    return std::pair<int,int>(-1,-1);
  }
}}}

// STATISTICS: int-branch