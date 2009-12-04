//== MemRegion.cpp - Abstract memory regions for static analysis --*- C++ -*--//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines MemRegion and its subclasses.  MemRegion defines a
//  partially-typed abstraction of memory useful for path-sensitive dataflow
//  analyses.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/raw_ostream.h"
#include "clang/Analysis/PathSensitive/MemRegion.h"
#include "clang/Analysis/PathSensitive/ValueManager.h"
#include "clang/Analysis/PathSensitive/AnalysisContext.h"
#include "clang/AST/StmtVisitor.h"

using namespace clang;

//===----------------------------------------------------------------------===//
// MemRegion Construction.
//===----------------------------------------------------------------------===//

template<typename RegionTy> struct MemRegionManagerTrait;

template <typename RegionTy, typename A1>
RegionTy* MemRegionManager::getRegion(const A1 a1) {
  
  const typename MemRegionManagerTrait<RegionTy>::SuperRegionTy *superRegion =
  MemRegionManagerTrait<RegionTy>::getSuperRegion(*this, a1);
  
  llvm::FoldingSetNodeID ID;
  RegionTy::ProfileRegion(ID, a1, superRegion);
  void* InsertPos;
  RegionTy* R = cast_or_null<RegionTy>(Regions.FindNodeOrInsertPos(ID,
                                                                   InsertPos));
  
  if (!R) {
    R = (RegionTy*) A.Allocate<RegionTy>();
    new (R) RegionTy(a1, superRegion);
    Regions.InsertNode(R, InsertPos);
  }
  
  return R;
}

template <typename RegionTy, typename A1>
RegionTy* MemRegionManager::getSubRegion(const A1 a1,
                                         const MemRegion *superRegion) {
  llvm::FoldingSetNodeID ID;
  RegionTy::ProfileRegion(ID, a1, superRegion);
  void* InsertPos;
  RegionTy* R = cast_or_null<RegionTy>(Regions.FindNodeOrInsertPos(ID,
                                                                   InsertPos));
  
  if (!R) {
    R = (RegionTy*) A.Allocate<RegionTy>();
    new (R) RegionTy(a1, superRegion);
    Regions.InsertNode(R, InsertPos);
  }
  
  return R;
}

template <typename RegionTy, typename A1, typename A2>
RegionTy* MemRegionManager::getRegion(const A1 a1, const A2 a2) {
  
  const typename MemRegionManagerTrait<RegionTy>::SuperRegionTy *superRegion =
  MemRegionManagerTrait<RegionTy>::getSuperRegion(*this, a1, a2);
  
  llvm::FoldingSetNodeID ID;
  RegionTy::ProfileRegion(ID, a1, a2, superRegion);
  void* InsertPos;
  RegionTy* R = cast_or_null<RegionTy>(Regions.FindNodeOrInsertPos(ID,
                                                                   InsertPos));
  
  if (!R) {
    R = (RegionTy*) A.Allocate<RegionTy>();
    new (R) RegionTy(a1, a2, superRegion);
    Regions.InsertNode(R, InsertPos);
  }
  
  return R;
}

template <typename RegionTy, typename A1, typename A2>
RegionTy* MemRegionManager::getSubRegion(const A1 a1, const A2 a2,
                                         const MemRegion *superRegion) {
  
  llvm::FoldingSetNodeID ID;
  RegionTy::ProfileRegion(ID, a1, a2, superRegion);
  void* InsertPos;
  RegionTy* R = cast_or_null<RegionTy>(Regions.FindNodeOrInsertPos(ID,
                                                                   InsertPos));
  
  if (!R) {
    R = (RegionTy*) A.Allocate<RegionTy>();
    new (R) RegionTy(a1, a2, superRegion);
    Regions.InsertNode(R, InsertPos);
  }
  
  return R;
}

//===----------------------------------------------------------------------===//
// Traits for constructing regions.
//===----------------------------------------------------------------------===//

template <> struct MemRegionManagerTrait<AllocaRegion> {
  typedef MemRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager& MRMgr,
                                             const Expr *, unsigned) {
    return MRMgr.getStackRegion();
  }
};

template <> struct MemRegionManagerTrait<CompoundLiteralRegion> {
  typedef MemRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager& MRMgr,
                                             const CompoundLiteralExpr *CL) {
    
    return CL->isFileScope() ? MRMgr.getGlobalsRegion()
    : MRMgr.getStackRegion();
  }
};

template <> struct MemRegionManagerTrait<StringRegion> {
  typedef MemSpaceRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager& MRMgr,
                                             const StringLiteral*) {
    return MRMgr.getGlobalsRegion();
  }
};

template <> struct MemRegionManagerTrait<VarRegion> {
  typedef MemRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager &MRMgr,
                                             const VarDecl *D,
                                             const LocationContext *LC) {
    
    // FIXME: Make stack regions have a location context?
    
    if (D->hasLocalStorage()) {
      return isa<ParmVarDecl>(D) || isa<ImplicitParamDecl>(D)
      ? MRMgr.getStackArgumentsRegion() : MRMgr.getStackRegion();
    }
    
    return MRMgr.getGlobalsRegion();
  }
};

template <> struct MemRegionManagerTrait<SymbolicRegion> {
  typedef MemRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager& MRMgr,
                                             SymbolRef) {
    return MRMgr.getUnknownRegion();
  }
};

template<> struct MemRegionManagerTrait<FunctionTextRegion> {
  typedef MemSpaceRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager& MRMgr,
                                             const FunctionDecl*) {
    return MRMgr.getCodeRegion();
  }
};
template<> struct MemRegionManagerTrait<BlockTextRegion> {
  typedef MemSpaceRegion SuperRegionTy;
  static const SuperRegionTy* getSuperRegion(MemRegionManager& MRMgr,
                                             const BlockDecl*, CanQualType) {
    return MRMgr.getCodeRegion();
  }
};

//===----------------------------------------------------------------------===//
// Object destruction.
//===----------------------------------------------------------------------===//

MemRegion::~MemRegion() {}

MemRegionManager::~MemRegionManager() {
  // All regions and their data are BumpPtrAllocated.  No need to call
  // their destructors.
}

//===----------------------------------------------------------------------===//
// Basic methods.
//===----------------------------------------------------------------------===//

bool SubRegion::isSubRegionOf(const MemRegion* R) const {
  const MemRegion* r = getSuperRegion();
  while (r != 0) {
    if (r == R)
      return true;
    if (const SubRegion* sr = dyn_cast<SubRegion>(r))
      r = sr->getSuperRegion();
    else
      break;
  }
  return false;
}

MemRegionManager* SubRegion::getMemRegionManager() const {
  const SubRegion* r = this;
  do {
    const MemRegion *superRegion = r->getSuperRegion();
    if (const SubRegion *sr = dyn_cast<SubRegion>(superRegion)) {
      r = sr;
      continue;
    }
    return superRegion->getMemRegionManager();
  } while (1);
}

void MemSpaceRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  ID.AddInteger((unsigned)getKind());
}

void StringRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                 const StringLiteral* Str,
                                 const MemRegion* superRegion) {
  ID.AddInteger((unsigned) StringRegionKind);
  ID.AddPointer(Str);
  ID.AddPointer(superRegion);
}

void AllocaRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                 const Expr* Ex, unsigned cnt,
                                 const MemRegion *) {
  ID.AddInteger((unsigned) AllocaRegionKind);
  ID.AddPointer(Ex);
  ID.AddInteger(cnt);
}

void AllocaRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  ProfileRegion(ID, Ex, Cnt, superRegion);
}

void CompoundLiteralRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  CompoundLiteralRegion::ProfileRegion(ID, CL, superRegion);
}

void CompoundLiteralRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                          const CompoundLiteralExpr* CL,
                                          const MemRegion* superRegion) {
  ID.AddInteger((unsigned) CompoundLiteralRegionKind);
  ID.AddPointer(CL);
  ID.AddPointer(superRegion);
}

void DeclRegion::ProfileRegion(llvm::FoldingSetNodeID& ID, const Decl* D,
                               const MemRegion* superRegion, Kind k) {
  ID.AddInteger((unsigned) k);
  ID.AddPointer(D);
  ID.AddPointer(superRegion);
}

void DeclRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  DeclRegion::ProfileRegion(ID, D, superRegion, getKind());
}

void VarRegion::Profile(llvm::FoldingSetNodeID &ID) const {
  VarRegion::ProfileRegion(ID, getDecl(), LC, superRegion);
}

void SymbolicRegion::ProfileRegion(llvm::FoldingSetNodeID& ID, SymbolRef sym,
                                   const MemRegion *sreg) {
  ID.AddInteger((unsigned) MemRegion::SymbolicRegionKind);
  ID.Add(sym);
  ID.AddPointer(sreg);
}

void SymbolicRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  SymbolicRegion::ProfileRegion(ID, sym, getSuperRegion());
}

void ElementRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                  QualType ElementType, SVal Idx,
                                  const MemRegion* superRegion) {
  ID.AddInteger(MemRegion::ElementRegionKind);
  ID.Add(ElementType);
  ID.AddPointer(superRegion);
  Idx.Profile(ID);
}

void ElementRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  ElementRegion::ProfileRegion(ID, ElementType, Index, superRegion);
}

void FunctionTextRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                       const FunctionDecl *FD,
                                       const MemRegion*) {
  ID.AddInteger(MemRegion::FunctionTextRegionKind);
  ID.AddPointer(FD);
}

void FunctionTextRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  FunctionTextRegion::ProfileRegion(ID, FD, superRegion);
}

void BlockTextRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                   const BlockDecl *BD, CanQualType,
                                   const MemRegion*) {
  ID.AddInteger(MemRegion::BlockTextRegionKind);
  ID.AddPointer(BD);
}

void BlockTextRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  BlockTextRegion::ProfileRegion(ID, BD, locTy, superRegion);
}

void BlockDataRegion::ProfileRegion(llvm::FoldingSetNodeID& ID,
                                    const BlockTextRegion *BC,
                                    const LocationContext *LC,
                                    const MemRegion *) {
  ID.AddInteger(MemRegion::BlockDataRegionKind);
  ID.AddPointer(BC);
  ID.AddPointer(LC);
}

void BlockDataRegion::Profile(llvm::FoldingSetNodeID& ID) const {
  BlockDataRegion::ProfileRegion(ID, BC, LC, NULL);
}

//===----------------------------------------------------------------------===//
// Region pretty-printing.
//===----------------------------------------------------------------------===//

void MemRegion::dump() const {
  dumpToStream(llvm::errs());
}

std::string MemRegion::getString() const {
  std::string s;
  llvm::raw_string_ostream os(s);
  dumpToStream(os);
  return os.str();
}

void MemRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "<Unknown Region>";
}

void AllocaRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "alloca{" << (void*) Ex << ',' << Cnt << '}';
}

void FunctionTextRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "code{" << getDecl()->getDeclName().getAsString() << '}';
}

void BlockTextRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "block_code{" << (void*) this << '}';
}

void BlockDataRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "block_data{" << BC << '}';
}


void CompoundLiteralRegion::dumpToStream(llvm::raw_ostream& os) const {
  // FIXME: More elaborate pretty-printing.
  os << "{ " << (void*) CL <<  " }";
}

void ElementRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "element{" << superRegion << ','
     << Index << ',' << getElementType().getAsString() << '}';
}

void FieldRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << superRegion << "->" << getDecl()->getNameAsString();
}

void ObjCIvarRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "ivar{" << superRegion << ',' << getDecl()->getNameAsString() << '}';
}

void StringRegion::dumpToStream(llvm::raw_ostream& os) const {
  Str->printPretty(os, 0, PrintingPolicy(getContext().getLangOptions()));
}

void SymbolicRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << "SymRegion{" << sym << '}';
}

void VarRegion::dumpToStream(llvm::raw_ostream& os) const {
  os << cast<VarDecl>(D)->getNameAsString();
}

void RegionRawOffset::dump() const {
  dumpToStream(llvm::errs());
}

void RegionRawOffset::dumpToStream(llvm::raw_ostream& os) const {
  os << "raw_offset{" << getRegion() << ',' << getByteOffset() << '}';
}

//===----------------------------------------------------------------------===//
// MemRegionManager methods.
//===----------------------------------------------------------------------===//

MemSpaceRegion* MemRegionManager::LazyAllocate(MemSpaceRegion*& region) {
  if (!region) {
    region = (MemSpaceRegion*) A.Allocate<MemSpaceRegion>();
    new (region) MemSpaceRegion(this);
  }

  return region;
}

MemSpaceRegion* MemRegionManager::getStackRegion() {
  return LazyAllocate(stack);
}

MemSpaceRegion* MemRegionManager::getStackArgumentsRegion() {
  return LazyAllocate(stackArguments);
}

MemSpaceRegion* MemRegionManager::getGlobalsRegion() {
  return LazyAllocate(globals);
}

MemSpaceRegion* MemRegionManager::getHeapRegion() {
  return LazyAllocate(heap);
}

MemSpaceRegion* MemRegionManager::getUnknownRegion() {
  return LazyAllocate(unknown);
}

MemSpaceRegion* MemRegionManager::getCodeRegion() {
  return LazyAllocate(code);
}

//===----------------------------------------------------------------------===//
// Constructing regions.
//===----------------------------------------------------------------------===//

StringRegion* MemRegionManager::getStringRegion(const StringLiteral* Str) {
  return getRegion<StringRegion>(Str);
}

VarRegion* MemRegionManager::getVarRegion(const VarDecl *D,
                                          const LocationContext *LC) {

  // FIXME: Once we implement scope handling, we will need to properly lookup
  // 'D' to the proper LocationContext.  For now, just strip down to the
  // StackFrame.
  while (!isa<StackFrameContext>(LC))
    LC = LC->getParent();

  return getRegion<VarRegion>(D, LC);
}

BlockDataRegion *MemRegionManager::getBlockDataRegion(const BlockTextRegion *BC,
                                                      const LocationContext *LC)
{
  // FIXME: Once we implement scope handling, we will need to properly lookup
  // 'D' to the proper LocationContext.  For now, just strip down to the
  // StackFrame.
  while (!isa<StackFrameContext>(LC))
    LC = LC->getParent();
  
  return getSubRegion<BlockDataRegion>(BC, LC, getStackRegion());
}

CompoundLiteralRegion*
MemRegionManager::getCompoundLiteralRegion(const CompoundLiteralExpr* CL) {
  return getRegion<CompoundLiteralRegion>(CL);
}

ElementRegion*
MemRegionManager::getElementRegion(QualType elementType, SVal Idx,
                                   const MemRegion* superRegion,
                                   ASTContext& Ctx){

  QualType T = Ctx.getCanonicalType(elementType);

  llvm::FoldingSetNodeID ID;
  ElementRegion::ProfileRegion(ID, T, Idx, superRegion);

  void* InsertPos;
  MemRegion* data = Regions.FindNodeOrInsertPos(ID, InsertPos);
  ElementRegion* R = cast_or_null<ElementRegion>(data);

  if (!R) {
    R = (ElementRegion*) A.Allocate<ElementRegion>();
    new (R) ElementRegion(T, Idx, superRegion);
    Regions.InsertNode(R, InsertPos);
  }

  return R;
}

FunctionTextRegion *
MemRegionManager::getFunctionTextRegion(const FunctionDecl *FD) {
  return getRegion<FunctionTextRegion>(FD);
}

BlockTextRegion *MemRegionManager::getBlockTextRegion(const BlockDecl *BD,
                                                      CanQualType locTy) {
  return getRegion<BlockTextRegion>(BD, locTy);
}


/// getSymbolicRegion - Retrieve or create a "symbolic" memory region.
SymbolicRegion* MemRegionManager::getSymbolicRegion(SymbolRef sym) {
  return getRegion<SymbolicRegion>(sym);
}

FieldRegion* MemRegionManager::getFieldRegion(const FieldDecl* d,
                                              const MemRegion* superRegion) {
  return getSubRegion<FieldRegion>(d, superRegion);
}

ObjCIvarRegion*
MemRegionManager::getObjCIvarRegion(const ObjCIvarDecl* d,
                                    const MemRegion* superRegion) {
  return getSubRegion<ObjCIvarRegion>(d, superRegion);
}

ObjCObjectRegion*
MemRegionManager::getObjCObjectRegion(const ObjCInterfaceDecl* d,
                                      const MemRegion* superRegion) {
  return getSubRegion<ObjCObjectRegion>(d, superRegion);
}

AllocaRegion* MemRegionManager::getAllocaRegion(const Expr* E, unsigned cnt) {
  return getRegion<AllocaRegion>(E, cnt);
}

const MemSpaceRegion *MemRegion::getMemorySpace() const {
  const MemRegion *R = this;
  const SubRegion* SR = dyn_cast<SubRegion>(this);

  while (SR) {
    R = SR->getSuperRegion();
    SR = dyn_cast<SubRegion>(R);
  }

  return dyn_cast<MemSpaceRegion>(R);
}

bool MemRegion::hasStackStorage() const {
  if (const MemSpaceRegion *MS = getMemorySpace()) {
    MemRegionManager *Mgr = getMemRegionManager();
    return MS == Mgr->getStackRegion() || MS == Mgr->getStackArgumentsRegion();
  }

  return false;
}

bool MemRegion::hasHeapStorage() const {
  if (const MemSpaceRegion *MS = getMemorySpace())
    return MS == getMemRegionManager()->getHeapRegion();

  return false;
}

bool MemRegion::hasHeapOrStackStorage() const {
  if (const MemSpaceRegion *MS = getMemorySpace()) {
    MemRegionManager *Mgr = getMemRegionManager();
    return MS == Mgr->getHeapRegion()
      || MS == Mgr->getStackRegion()
      || MS == Mgr->getStackArgumentsRegion();
  }
  return false;
}

bool MemRegion::hasGlobalsStorage() const {
  if (const MemSpaceRegion *MS = getMemorySpace())
    return MS == getMemRegionManager()->getGlobalsRegion();

  return false;
}

bool MemRegion::hasParametersStorage() const {
  if (const MemSpaceRegion *MS = getMemorySpace())
    return MS == getMemRegionManager()->getStackArgumentsRegion();

  return false;
}

bool MemRegion::hasGlobalsOrParametersStorage() const {
  if (const MemSpaceRegion *MS = getMemorySpace()) {
    MemRegionManager *Mgr = getMemRegionManager();
    return MS == Mgr->getGlobalsRegion()
    || MS == Mgr->getStackArgumentsRegion();
  }
  return false;
}

// getBaseRegion strips away all elements and fields, and get the base region
// of them.
const MemRegion *MemRegion::getBaseRegion() const {
  const MemRegion *R = this;
  while (true) {
    if (const ElementRegion *ER = dyn_cast<ElementRegion>(R)) {
      R = ER->getSuperRegion();
      continue;
    }
    if (const FieldRegion *FR = dyn_cast<FieldRegion>(R)) {
      R = FR->getSuperRegion();
      continue;
    }
    break;
  }
  return R;
}

//===----------------------------------------------------------------------===//
// View handling.
//===----------------------------------------------------------------------===//

const MemRegion *MemRegion::StripCasts() const {
  const MemRegion *R = this;
  while (true) {
    if (const ElementRegion *ER = dyn_cast<ElementRegion>(R)) {
      // FIXME: generalize.  Essentially we want to strip away ElementRegions
      // that were layered on a symbolic region because of casts.  We only
      // want to strip away ElementRegions, however, where the index is 0.
      SVal index = ER->getIndex();
      if (nonloc::ConcreteInt *CI = dyn_cast<nonloc::ConcreteInt>(&index)) {
        if (CI->getValue().getSExtValue() == 0) {
          R = ER->getSuperRegion();
          continue;
        }
      }
    }
    break;
  }
  return R;
}

// FIXME: Merge with the implementation of the same method in Store.cpp
static bool IsCompleteType(ASTContext &Ctx, QualType Ty) {
  if (const RecordType *RT = Ty->getAs<RecordType>()) {
    const RecordDecl *D = RT->getDecl();
    if (!D->getDefinition(Ctx))
      return false;
  }

  return true;
}

RegionRawOffset ElementRegion::getAsRawOffset() const {
  int64_t offset = 0;
  const ElementRegion *ER = this;
  const MemRegion *superR = NULL;
  ASTContext &C = getContext();

  // FIXME: Handle multi-dimensional arrays.

  while (ER) {
    superR = ER->getSuperRegion();

    // FIXME: generalize to symbolic offsets.
    SVal index = ER->getIndex();
    if (nonloc::ConcreteInt *CI = dyn_cast<nonloc::ConcreteInt>(&index)) {
      // Update the offset.
      int64_t i = CI->getValue().getSExtValue();

      if (i != 0) {
        QualType elemType = ER->getElementType();

        // If we are pointing to an incomplete type, go no further.
        if (!IsCompleteType(C, elemType)) {
          superR = ER;
          break;
        }

        int64_t size = (int64_t) (C.getTypeSize(elemType) / 8);
        offset += (i * size);
      }

      // Go to the next ElementRegion (if any).
      ER = dyn_cast<ElementRegion>(superR);
      continue;
    }

    return NULL;
  }

  assert(superR && "super region cannot be NULL");
  return RegionRawOffset(superR, offset);
}

//===----------------------------------------------------------------------===//
// BlockDataRegion
//===----------------------------------------------------------------------===//

void BlockDataRegion::LazyInitializeReferencedVars() {
  if (ReferencedVars)
    return;

  AnalysisContext *AC = LC->getAnalysisContext();
  AnalysisContext::referenced_decls_iterator I, E;
  llvm::tie(I, E) = AC->getReferencedBlockVars(BC->getDecl());
  
  if (I == E) {
    ReferencedVars = (void*) 0x1;
    return;
  }
    
  MemRegionManager &MemMgr = *getMemRegionManager();
  llvm::BumpPtrAllocator &A = MemMgr.getAllocator();
  BumpVectorContext BC(A);
  
  typedef BumpVector<const MemRegion*> VarVec;
  VarVec *BV = (VarVec*) A.Allocate<VarVec>();
  new (BV) VarVec(BC, E - I);
  
  for ( ; I != E; ++I)
    BV->push_back(MemMgr.getVarRegion(*I, LC), BC);
  
  ReferencedVars = BV;
}

BlockDataRegion::referenced_vars_iterator
BlockDataRegion::referenced_vars_begin() const {
  const_cast<BlockDataRegion*>(this)->LazyInitializeReferencedVars();

  BumpVector<const MemRegion*> *Vec =
    static_cast<BumpVector<const MemRegion*>*>(ReferencedVars);
  
  return BlockDataRegion::referenced_vars_iterator(Vec == (void*) 0x1 ?
                                                   NULL : Vec->begin());
}

BlockDataRegion::referenced_vars_iterator
BlockDataRegion::referenced_vars_end() const {
  const_cast<BlockDataRegion*>(this)->LazyInitializeReferencedVars();

  BumpVector<const MemRegion*> *Vec =
    static_cast<BumpVector<const MemRegion*>*>(ReferencedVars);
  
  return BlockDataRegion::referenced_vars_iterator(Vec == (void*) 0x1 ?
                                                   NULL : Vec->end());
}
