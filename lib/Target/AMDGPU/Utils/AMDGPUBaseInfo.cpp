//===- AMDGPUBaseInfo.cpp - AMDGPU Base encoding information --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "AMDGPUBaseInfo.h"
#include "AMDGPU.h"
#include "SIDefines.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>

#include "MCTargetDesc/AMDGPUMCTargetDesc.h"

#define GET_INSTRINFO_NAMED_OPS
#define GET_INSTRMAP_INFO
#include "AMDGPUGenInstrInfo.inc"
#undef GET_INSTRMAP_INFO
#undef GET_INSTRINFO_NAMED_OPS

namespace {

/// \returns Bit mask for given bit \p Shift and bit \p Width.
unsigned getBitMask(unsigned Shift, unsigned Width) {
  return ((1 << Width) - 1) << Shift;
}

/// \brief Packs \p Src into \p Dst for given bit \p Shift and bit \p Width.
///
/// \returns Packed \p Dst.
unsigned packBits(unsigned Src, unsigned Dst, unsigned Shift, unsigned Width) {
  Dst &= ~(1 << Shift) & ~getBitMask(Shift, Width);
  Dst |= (Src << Shift) & getBitMask(Shift, Width);
  return Dst;
}

/// \brief Unpacks bits from \p Src for given bit \p Shift and bit \p Width.
///
/// \returns Unpacked bits.
unsigned unpackBits(unsigned Src, unsigned Shift, unsigned Width) {
  return (Src & getBitMask(Shift, Width)) >> Shift;
}

/// \returns Vmcnt bit shift (lower bits).
unsigned getVmcntBitShiftLo() { return 0; }

/// \returns Vmcnt bit width (lower bits).
unsigned getVmcntBitWidthLo() { return 4; }

/// \returns Expcnt bit shift.
unsigned getExpcntBitShift() { return 4; }

/// \returns Expcnt bit width.
unsigned getExpcntBitWidth() { return 3; }

/// \returns Lgkmcnt bit shift.
unsigned getLgkmcntBitShift() { return 8; }

/// \returns Lgkmcnt bit width.
unsigned getLgkmcntBitWidth() { return 4; }

/// \returns Vmcnt bit shift (higher bits).
unsigned getVmcntBitShiftHi() { return 14; }

/// \returns Vmcnt bit width (higher bits).
unsigned getVmcntBitWidthHi() { return 2; }

} // end namespace anonymous

namespace llvm {

static cl::opt<bool> EnablePackedInlinableLiterals(
    "enable-packed-inlinable-literals",
    cl::desc("Enable packed inlinable literals (v2f16, v2i16)"),
    cl::init(false));

namespace AMDGPU {

LLVM_READNONE
static inline Channels indexToChannel(unsigned Channel) {
  switch (Channel) {
  case 1:
    return AMDGPU::Channels_1;
  case 2:
    return AMDGPU::Channels_2;
  case 3:
    return AMDGPU::Channels_3;
  case 4:
    return AMDGPU::Channels_4;
  default:
    llvm_unreachable("invalid MIMG channel");
  }
}


// FIXME: Need to handle d16 images correctly.
static unsigned rcToChannels(unsigned RCID) {
  switch (RCID) {
  case AMDGPU::VGPR_32RegClassID:
    return 1;
  case AMDGPU::VReg_64RegClassID:
    return 2;
  case AMDGPU::VReg_96RegClassID:
    return 3;
  case AMDGPU::VReg_128RegClassID:
    return 4;
  default:
    llvm_unreachable("invalid MIMG register class");
  }
}

int getMaskedMIMGOp(const MCInstrInfo &MII, unsigned Opc, unsigned NewChannels) {
  AMDGPU::Channels Channel = AMDGPU::indexToChannel(NewChannels);
  unsigned OrigChannels = rcToChannels(MII.get(Opc).OpInfo[0].RegClass);
  if (NewChannels == OrigChannels)
    return Opc;

  switch (OrigChannels) {
  case 1:
    return AMDGPU::getMaskedMIMGOp1(Opc, Channel);
  case 2:
    return AMDGPU::getMaskedMIMGOp2(Opc, Channel);
  case 3:
    return AMDGPU::getMaskedMIMGOp3(Opc, Channel);
  case 4:
    return AMDGPU::getMaskedMIMGOp4(Opc, Channel);
  default:
    llvm_unreachable("invalid MIMG channel");
  }
}

int getMaskedMIMGAtomicOp(const MCInstrInfo &MII, unsigned Opc, unsigned NewChannels) {
  assert(AMDGPU::getNamedOperandIdx(Opc, AMDGPU::OpName::vdst) != -1);
  assert(NewChannels == 1 || NewChannels == 2 || NewChannels == 4);

  unsigned OrigChannels = rcToChannels(MII.get(Opc).OpInfo[0].RegClass);
  assert(OrigChannels == 1 || OrigChannels == 2 || OrigChannels == 4);

  if (NewChannels == OrigChannels) return Opc;

  if (OrigChannels <= 2 && NewChannels <= 2) {
    // This is an ordinary atomic (not an atomic_cmpswap)
    return (OrigChannels == 1)?
      AMDGPU::getMIMGAtomicOp1(Opc) : AMDGPU::getMIMGAtomicOp2(Opc);
  } else if (OrigChannels >= 2 && NewChannels >= 2) {
    // This is an atomic_cmpswap
    return (OrigChannels == 2)?
      AMDGPU::getMIMGAtomicOp1(Opc) : AMDGPU::getMIMGAtomicOp2(Opc);
  } else { // invalid OrigChannels/NewChannels value
    return -1;
  }
}

// Wrapper for Tablegen'd function.  enum Subtarget is not defined in any
// header files, so we need to wrap it in a function that takes unsigned
// instead.
int getMCOpcode(uint16_t Opcode, unsigned Gen) {
  return getMCOpcodeGen(Opcode, static_cast<Subtarget>(Gen));
}

namespace IsaInfo {

IsaVersion getIsaVersion(const FeatureBitset &Features) {
  // GCN GFX6 (Southern Islands (SI)).
  if (Features.test(FeatureISAVersion6_0_0))
    return {6, 0, 0};
  if (Features.test(FeatureISAVersion6_0_1))
    return {6, 0, 1};

  // GCN GFX7 (Sea Islands (CI)).
  if (Features.test(FeatureISAVersion7_0_0))
    return {7, 0, 0};
  if (Features.test(FeatureISAVersion7_0_1))
    return {7, 0, 1};
  if (Features.test(FeatureISAVersion7_0_2))
    return {7, 0, 2};
  if (Features.test(FeatureISAVersion7_0_3))
    return {7, 0, 3};
  if (Features.test(FeatureISAVersion7_0_4))
    return {7, 0, 4};

  // GCN GFX8 (Volcanic Islands (VI)).
  if (Features.test(FeatureISAVersion8_0_0))
    return {8, 0, 0};
  if (Features.test(FeatureISAVersion8_0_1))
    return {8, 0, 1};
  if (Features.test(FeatureISAVersion8_0_2))
    return {8, 0, 2};
  if (Features.test(FeatureISAVersion8_0_3))
    return {8, 0, 3};
  if (Features.test(FeatureISAVersion8_1_0))
    return {8, 1, 0};

  // GCN GFX9.
  if (Features.test(FeatureISAVersion9_0_0))
    return {9, 0, 0};
  if (Features.test(FeatureISAVersion9_0_2))
    return {9, 0, 2};

  if (!Features.test(FeatureGCN) || Features.test(FeatureSouthernIslands))
    return {0, 0, 0};
  return {7, 0, 0};
}

void streamIsaVersion(const MCSubtargetInfo *STI, raw_ostream &Stream) {
  auto TargetTriple = STI->getTargetTriple();
  auto ISAVersion = IsaInfo::getIsaVersion(STI->getFeatureBits());

  Stream << TargetTriple.getArchName() << '-'
         << TargetTriple.getVendorName() << '-'
         << TargetTriple.getOSName() << '-'
         << TargetTriple.getEnvironmentName() << '-'
         << "gfx"
         << ISAVersion.Major
         << ISAVersion.Minor
         << ISAVersion.Stepping;
  Stream.flush();
}

bool hasCodeObjectV3(const FeatureBitset &Features) {
  return Features.test(FeatureCodeObjectV3);
}

unsigned getWavefrontSize(const FeatureBitset &Features) {
  if (Features.test(FeatureWavefrontSize16))
    return 16;
  if (Features.test(FeatureWavefrontSize32))
    return 32;

  return 64;
}

unsigned getLocalMemorySize(const FeatureBitset &Features) {
  if (Features.test(FeatureLocalMemorySize32768))
    return 32768;
  if (Features.test(FeatureLocalMemorySize65536))
    return 65536;

  return 0;
}

unsigned getEUsPerCU(const FeatureBitset &Features) {
  return 4;
}

unsigned getMaxWorkGroupsPerCU(const FeatureBitset &Features,
                               unsigned FlatWorkGroupSize) {
  if (!Features.test(FeatureGCN))
    return 8;
  unsigned N = getWavesPerWorkGroup(Features, FlatWorkGroupSize);
  if (N == 1)
    return 40;
  N = 40 / N;
  return std::min(N, 16u);
}

unsigned getMaxWavesPerCU(const FeatureBitset &Features) {
  return getMaxWavesPerEU(Features) * getEUsPerCU(Features);
}

unsigned getMaxWavesPerCU(const FeatureBitset &Features,
                          unsigned FlatWorkGroupSize) {
  return getWavesPerWorkGroup(Features, FlatWorkGroupSize);
}

unsigned getMinWavesPerEU(const FeatureBitset &Features) {
  return 1;
}

unsigned getMaxWavesPerEU(const FeatureBitset &Features) {
  if (!Features.test(FeatureGCN))
    return 8;
  // FIXME: Need to take scratch memory into account.
  return 10;
}

unsigned getMaxWavesPerEU(const FeatureBitset &Features,
                          unsigned FlatWorkGroupSize) {
  return alignTo(getMaxWavesPerCU(Features, FlatWorkGroupSize),
                 getEUsPerCU(Features)) / getEUsPerCU(Features);
}

unsigned getMinFlatWorkGroupSize(const FeatureBitset &Features) {
  return 1;
}

unsigned getMaxFlatWorkGroupSize(const FeatureBitset &Features) {
  return 2048;
}

unsigned getWavesPerWorkGroup(const FeatureBitset &Features,
                              unsigned FlatWorkGroupSize) {
  return alignTo(FlatWorkGroupSize, getWavefrontSize(Features)) /
                 getWavefrontSize(Features);
}

unsigned getSGPRAllocGranule(const FeatureBitset &Features) {
  IsaVersion Version = getIsaVersion(Features);
  if (Version.Major >= 8)
    return 16;
  return 8;
}

unsigned getSGPREncodingGranule(const FeatureBitset &Features) {
  return 8;
}

unsigned getTotalNumSGPRs(const FeatureBitset &Features) {
  IsaVersion Version = getIsaVersion(Features);
  if (Version.Major >= 8)
    return 800;
  return 512;
}

unsigned getAddressableNumSGPRs(const FeatureBitset &Features) {
  if (Features.test(FeatureSGPRInitBug))
    return FIXED_NUM_SGPRS_FOR_INIT_BUG;

  IsaVersion Version = getIsaVersion(Features);
  if (Version.Major >= 8)
    return 102;
  return 104;
}

unsigned getMinNumSGPRs(const FeatureBitset &Features, unsigned WavesPerEU) {
  assert(WavesPerEU != 0);

  if (WavesPerEU >= getMaxWavesPerEU(Features))
    return 0;
  unsigned MinNumSGPRs =
      alignDown(getTotalNumSGPRs(Features) / (WavesPerEU + 1),
                getSGPRAllocGranule(Features)) + 1;
  return std::min(MinNumSGPRs, getAddressableNumSGPRs(Features));
}

unsigned getMaxNumSGPRs(const FeatureBitset &Features, unsigned WavesPerEU,
                        bool Addressable) {
  assert(WavesPerEU != 0);

  IsaVersion Version = getIsaVersion(Features);
  unsigned MaxNumSGPRs = alignDown(getTotalNumSGPRs(Features) / WavesPerEU,
                                   getSGPRAllocGranule(Features));
  unsigned AddressableNumSGPRs = getAddressableNumSGPRs(Features);
  if (Version.Major >= 8 && !Addressable)
    AddressableNumSGPRs = 112;
  return std::min(MaxNumSGPRs, AddressableNumSGPRs);
}

unsigned getVGPRAllocGranule(const FeatureBitset &Features) {
  return 4;
}

unsigned getVGPREncodingGranule(const FeatureBitset &Features) {
  return getVGPRAllocGranule(Features);
}

unsigned getTotalNumVGPRs(const FeatureBitset &Features) {
  return 256;
}

unsigned getAddressableNumVGPRs(const FeatureBitset &Features) {
  return getTotalNumVGPRs(Features);
}

unsigned getMinNumVGPRs(const FeatureBitset &Features, unsigned WavesPerEU) {
  assert(WavesPerEU != 0);

  if (WavesPerEU >= getMaxWavesPerEU(Features))
    return 0;
  unsigned MinNumVGPRs =
      alignDown(getTotalNumVGPRs(Features) / (WavesPerEU + 1),
                getVGPRAllocGranule(Features)) + 1;
  return std::min(MinNumVGPRs, getAddressableNumVGPRs(Features));
}

unsigned getMaxNumVGPRs(const FeatureBitset &Features, unsigned WavesPerEU) {
  assert(WavesPerEU != 0);

  unsigned MaxNumVGPRs = alignDown(getTotalNumVGPRs(Features) / WavesPerEU,
                                   getVGPRAllocGranule(Features));
  unsigned AddressableNumVGPRs = getAddressableNumVGPRs(Features);
  return std::min(MaxNumVGPRs, AddressableNumVGPRs);
}

} // end namespace IsaInfo

void initDefaultAMDKernelCodeT(amd_kernel_code_t &Header,
                               const FeatureBitset &Features) {
  IsaInfo::IsaVersion ISA = IsaInfo::getIsaVersion(Features);

  memset(&Header, 0, sizeof(Header));

  Header.amd_kernel_code_version_major = 1;
  Header.amd_kernel_code_version_minor = 1;
  Header.amd_machine_kind = 1; // AMD_MACHINE_KIND_AMDGPU
  Header.amd_machine_version_major = ISA.Major;
  Header.amd_machine_version_minor = ISA.Minor;
  Header.amd_machine_version_stepping = ISA.Stepping;
  Header.kernel_code_entry_byte_offset = sizeof(Header);
  // wavefront_size is specified as a power of 2: 2^6 = 64 threads.
  Header.wavefront_size = 6;

  // If the code object does not support indirect functions, then the value must
  // be 0xffffffff.
  Header.call_convention = -1;

  // These alignment values are specified in powers of two, so alignment =
  // 2^n.  The minimum alignment is 2^4 = 16.
  Header.kernarg_segment_alignment = 4;
  Header.group_segment_alignment = 4;
  Header.private_segment_alignment = 4;
}

bool isGroupSegment(const GlobalValue *GV) {
  return GV->getType()->getAddressSpace() == AMDGPUAS::LOCAL_ADDRESS;
}

bool isGlobalSegment(const GlobalValue *GV) {
  return GV->getType()->getAddressSpace() == AMDGPUAS::GLOBAL_ADDRESS;
}

bool isReadOnlySegment(const GlobalValue *GV) {
  return GV->getType()->getAddressSpace() == AMDGPUAS::CONSTANT_ADDRESS ||
         GV->getType()->getAddressSpace() == AMDGPUAS::CONSTANT_ADDRESS_32BIT;
}

bool shouldEmitConstantsToTextSection(const Triple &TT) {
  return TT.getOS() != Triple::AMDHSA;
}

int getIntegerAttribute(const Function &F, StringRef Name, int Default) {
  Attribute A = F.getFnAttribute(Name);
  int Result = Default;

  if (A.isStringAttribute()) {
    StringRef Str = A.getValueAsString();
    if (Str.getAsInteger(0, Result)) {
      LLVMContext &Ctx = F.getContext();
      Ctx.emitError("can't parse integer attribute " + Name);
    }
  }

  return Result;
}

std::pair<int, int> getIntegerPairAttribute(const Function &F,
                                            StringRef Name,
                                            std::pair<int, int> Default,
                                            bool OnlyFirstRequired) {
  Attribute A = F.getFnAttribute(Name);
  if (!A.isStringAttribute())
    return Default;

  LLVMContext &Ctx = F.getContext();
  std::pair<int, int> Ints = Default;
  std::pair<StringRef, StringRef> Strs = A.getValueAsString().split(',');
  if (Strs.first.trim().getAsInteger(0, Ints.first)) {
    Ctx.emitError("can't parse first integer attribute " + Name);
    return Default;
  }
  if (Strs.second.trim().getAsInteger(0, Ints.second)) {
    if (!OnlyFirstRequired || !Strs.second.trim().empty()) {
      Ctx.emitError("can't parse second integer attribute " + Name);
      return Default;
    }
  }

  return Ints;
}

unsigned getVmcntBitMask(const IsaInfo::IsaVersion &Version) {
  unsigned VmcntLo = (1 << getVmcntBitWidthLo()) - 1;
  if (Version.Major < 9)
    return VmcntLo;

  unsigned VmcntHi = ((1 << getVmcntBitWidthHi()) - 1) << getVmcntBitWidthLo();
  return VmcntLo | VmcntHi;
}

unsigned getExpcntBitMask(const IsaInfo::IsaVersion &Version) {
  return (1 << getExpcntBitWidth()) - 1;
}

unsigned getLgkmcntBitMask(const IsaInfo::IsaVersion &Version) {
  return (1 << getLgkmcntBitWidth()) - 1;
}

unsigned getWaitcntBitMask(const IsaInfo::IsaVersion &Version) {
  unsigned VmcntLo = getBitMask(getVmcntBitShiftLo(), getVmcntBitWidthLo());
  unsigned Expcnt = getBitMask(getExpcntBitShift(), getExpcntBitWidth());
  unsigned Lgkmcnt = getBitMask(getLgkmcntBitShift(), getLgkmcntBitWidth());
  unsigned Waitcnt = VmcntLo | Expcnt | Lgkmcnt;
  if (Version.Major < 9)
    return Waitcnt;

  unsigned VmcntHi = getBitMask(getVmcntBitShiftHi(), getVmcntBitWidthHi());
  return Waitcnt | VmcntHi;
}

unsigned decodeVmcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt) {
  unsigned VmcntLo =
      unpackBits(Waitcnt, getVmcntBitShiftLo(), getVmcntBitWidthLo());
  if (Version.Major < 9)
    return VmcntLo;

  unsigned VmcntHi =
      unpackBits(Waitcnt, getVmcntBitShiftHi(), getVmcntBitWidthHi());
  VmcntHi <<= getVmcntBitWidthLo();
  return VmcntLo | VmcntHi;
}

unsigned decodeExpcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt) {
  return unpackBits(Waitcnt, getExpcntBitShift(), getExpcntBitWidth());
}

unsigned decodeLgkmcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt) {
  return unpackBits(Waitcnt, getLgkmcntBitShift(), getLgkmcntBitWidth());
}

void decodeWaitcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt,
                   unsigned &Vmcnt, unsigned &Expcnt, unsigned &Lgkmcnt) {
  Vmcnt = decodeVmcnt(Version, Waitcnt);
  Expcnt = decodeExpcnt(Version, Waitcnt);
  Lgkmcnt = decodeLgkmcnt(Version, Waitcnt);
}

unsigned encodeVmcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt,
                     unsigned Vmcnt) {
  Waitcnt =
      packBits(Vmcnt, Waitcnt, getVmcntBitShiftLo(), getVmcntBitWidthLo());
  if (Version.Major < 9)
    return Waitcnt;

  Vmcnt >>= getVmcntBitWidthLo();
  return packBits(Vmcnt, Waitcnt, getVmcntBitShiftHi(), getVmcntBitWidthHi());
}

unsigned encodeExpcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt,
                      unsigned Expcnt) {
  return packBits(Expcnt, Waitcnt, getExpcntBitShift(), getExpcntBitWidth());
}

unsigned encodeLgkmcnt(const IsaInfo::IsaVersion &Version, unsigned Waitcnt,
                       unsigned Lgkmcnt) {
  return packBits(Lgkmcnt, Waitcnt, getLgkmcntBitShift(), getLgkmcntBitWidth());
}

unsigned encodeWaitcnt(const IsaInfo::IsaVersion &Version,
                       unsigned Vmcnt, unsigned Expcnt, unsigned Lgkmcnt) {
  unsigned Waitcnt = getWaitcntBitMask(Version);
  Waitcnt = encodeVmcnt(Version, Waitcnt, Vmcnt);
  Waitcnt = encodeExpcnt(Version, Waitcnt, Expcnt);
  Waitcnt = encodeLgkmcnt(Version, Waitcnt, Lgkmcnt);
  return Waitcnt;
}

unsigned getInitialPSInputAddr(const Function &F) {
  return getIntegerAttribute(F, "InitialPSInputAddr", 0);
}

bool isShader(CallingConv::ID cc) {
  switch(cc) {
    case CallingConv::AMDGPU_VS:
    case CallingConv::AMDGPU_LS:
    case CallingConv::AMDGPU_HS:
    case CallingConv::AMDGPU_ES:
    case CallingConv::AMDGPU_GS:
    case CallingConv::AMDGPU_PS:
    case CallingConv::AMDGPU_CS:
      return true;
    default:
      return false;
  }
}

bool isCompute(CallingConv::ID cc) {
  return !isShader(cc) || cc == CallingConv::AMDGPU_CS;
}

bool isEntryFunctionCC(CallingConv::ID CC) {
  switch (CC) {
  case CallingConv::AMDGPU_KERNEL:
  case CallingConv::SPIR_KERNEL:
  case CallingConv::AMDGPU_VS:
  case CallingConv::AMDGPU_GS:
  case CallingConv::AMDGPU_PS:
  case CallingConv::AMDGPU_CS:
  case CallingConv::AMDGPU_ES:
  case CallingConv::AMDGPU_HS:
  case CallingConv::AMDGPU_LS:
    return true;
  default:
    return false;
  }
}

bool hasXNACK(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureXNACK];
}

bool hasMIMG_R128(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureMIMG_R128];
}

bool hasPackedD16(const MCSubtargetInfo &STI) {
  return !STI.getFeatureBits()[AMDGPU::FeatureUnpackedD16VMem];
}

bool isSI(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureSouthernIslands];
}

bool isCI(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureSeaIslands];
}

bool isVI(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureVolcanicIslands];
}

bool isGFX9(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureGFX9];
}

bool isGCN3Encoding(const MCSubtargetInfo &STI) {
  return STI.getFeatureBits()[AMDGPU::FeatureGCN3Encoding];
}

bool isSGPR(unsigned Reg, const MCRegisterInfo* TRI) {
  const MCRegisterClass SGPRClass = TRI->getRegClass(AMDGPU::SReg_32RegClassID);
  const unsigned FirstSubReg = TRI->getSubReg(Reg, 1);
  return SGPRClass.contains(FirstSubReg != 0 ? FirstSubReg : Reg) ||
    Reg == AMDGPU::SCC;
}

bool isRegIntersect(unsigned Reg0, unsigned Reg1, const MCRegisterInfo* TRI) {
  for (MCRegAliasIterator R(Reg0, TRI, true); R.isValid(); ++R) {
    if (*R == Reg1) return true;
  }
  return false;
}

#define MAP_REG2REG \
  using namespace AMDGPU; \
  switch(Reg) { \
  default: return Reg; \
  CASE_CI_VI(FLAT_SCR) \
  CASE_CI_VI(FLAT_SCR_LO) \
  CASE_CI_VI(FLAT_SCR_HI) \
  CASE_VI_GFX9(TTMP0) \
  CASE_VI_GFX9(TTMP1) \
  CASE_VI_GFX9(TTMP2) \
  CASE_VI_GFX9(TTMP3) \
  CASE_VI_GFX9(TTMP4) \
  CASE_VI_GFX9(TTMP5) \
  CASE_VI_GFX9(TTMP6) \
  CASE_VI_GFX9(TTMP7) \
  CASE_VI_GFX9(TTMP8) \
  CASE_VI_GFX9(TTMP9) \
  CASE_VI_GFX9(TTMP10) \
  CASE_VI_GFX9(TTMP11) \
  CASE_VI_GFX9(TTMP12) \
  CASE_VI_GFX9(TTMP13) \
  CASE_VI_GFX9(TTMP14) \
  CASE_VI_GFX9(TTMP15) \
  CASE_VI_GFX9(TTMP0_TTMP1) \
  CASE_VI_GFX9(TTMP2_TTMP3) \
  CASE_VI_GFX9(TTMP4_TTMP5) \
  CASE_VI_GFX9(TTMP6_TTMP7) \
  CASE_VI_GFX9(TTMP8_TTMP9) \
  CASE_VI_GFX9(TTMP10_TTMP11) \
  CASE_VI_GFX9(TTMP12_TTMP13) \
  CASE_VI_GFX9(TTMP14_TTMP15) \
  CASE_VI_GFX9(TTMP0_TTMP1_TTMP2_TTMP3) \
  CASE_VI_GFX9(TTMP4_TTMP5_TTMP6_TTMP7) \
  CASE_VI_GFX9(TTMP8_TTMP9_TTMP10_TTMP11) \
  CASE_VI_GFX9(TTMP12_TTMP13_TTMP14_TTMP15) \
  CASE_VI_GFX9(TTMP0_TTMP1_TTMP2_TTMP3_TTMP4_TTMP5_TTMP6_TTMP7) \
  CASE_VI_GFX9(TTMP4_TTMP5_TTMP6_TTMP7_TTMP8_TTMP9_TTMP10_TTMP11) \
  CASE_VI_GFX9(TTMP8_TTMP9_TTMP10_TTMP11_TTMP12_TTMP13_TTMP14_TTMP15) \
  CASE_VI_GFX9(TTMP0_TTMP1_TTMP2_TTMP3_TTMP4_TTMP5_TTMP6_TTMP7_TTMP8_TTMP9_TTMP10_TTMP11_TTMP12_TTMP13_TTMP14_TTMP15) \
  }

#define CASE_CI_VI(node) \
  assert(!isSI(STI)); \
  case node: return isCI(STI) ? node##_ci : node##_vi;

#define CASE_VI_GFX9(node) \
  case node: return isGFX9(STI) ? node##_gfx9 : node##_vi;

unsigned getMCReg(unsigned Reg, const MCSubtargetInfo &STI) {
  MAP_REG2REG
}

#undef CASE_CI_VI
#undef CASE_VI_GFX9

#define CASE_CI_VI(node)   case node##_ci: case node##_vi:   return node;
#define CASE_VI_GFX9(node) case node##_vi: case node##_gfx9: return node;

unsigned mc2PseudoReg(unsigned Reg) {
  MAP_REG2REG
}

#undef CASE_CI_VI
#undef CASE_VI_GFX9
#undef MAP_REG2REG

bool isSISrcOperand(const MCInstrDesc &Desc, unsigned OpNo) {
  assert(OpNo < Desc.NumOperands);
  unsigned OpType = Desc.OpInfo[OpNo].OperandType;
  return OpType >= AMDGPU::OPERAND_SRC_FIRST &&
         OpType <= AMDGPU::OPERAND_SRC_LAST;
}

bool isSISrcFPOperand(const MCInstrDesc &Desc, unsigned OpNo) {
  assert(OpNo < Desc.NumOperands);
  unsigned OpType = Desc.OpInfo[OpNo].OperandType;
  switch (OpType) {
  case AMDGPU::OPERAND_REG_IMM_FP32:
  case AMDGPU::OPERAND_REG_IMM_FP64:
  case AMDGPU::OPERAND_REG_IMM_FP16:
  case AMDGPU::OPERAND_REG_INLINE_C_FP32:
  case AMDGPU::OPERAND_REG_INLINE_C_FP64:
  case AMDGPU::OPERAND_REG_INLINE_C_FP16:
  case AMDGPU::OPERAND_REG_INLINE_C_V2FP16:
    return true;
  default:
    return false;
  }
}

bool isSISrcInlinableOperand(const MCInstrDesc &Desc, unsigned OpNo) {
  assert(OpNo < Desc.NumOperands);
  unsigned OpType = Desc.OpInfo[OpNo].OperandType;
  return OpType >= AMDGPU::OPERAND_REG_INLINE_C_FIRST &&
         OpType <= AMDGPU::OPERAND_REG_INLINE_C_LAST;
}

// Avoid using MCRegisterClass::getSize, since that function will go away
// (move from MC* level to Target* level). Return size in bits.
unsigned getRegBitWidth(unsigned RCID) {
  switch (RCID) {
  case AMDGPU::SGPR_32RegClassID:
  case AMDGPU::VGPR_32RegClassID:
  case AMDGPU::VS_32RegClassID:
  case AMDGPU::SReg_32RegClassID:
  case AMDGPU::SReg_32_XM0RegClassID:
    return 32;
  case AMDGPU::SGPR_64RegClassID:
  case AMDGPU::VS_64RegClassID:
  case AMDGPU::SReg_64RegClassID:
  case AMDGPU::VReg_64RegClassID:
    return 64;
  case AMDGPU::VReg_96RegClassID:
    return 96;
  case AMDGPU::SGPR_128RegClassID:
  case AMDGPU::SReg_128RegClassID:
  case AMDGPU::VReg_128RegClassID:
    return 128;
  case AMDGPU::SReg_256RegClassID:
  case AMDGPU::VReg_256RegClassID:
    return 256;
  case AMDGPU::SReg_512RegClassID:
  case AMDGPU::VReg_512RegClassID:
    return 512;
  default:
    llvm_unreachable("Unexpected register class");
  }
}

unsigned getRegBitWidth(const MCRegisterClass &RC) {
  return getRegBitWidth(RC.getID());
}

unsigned getRegOperandSize(const MCRegisterInfo *MRI, const MCInstrDesc &Desc,
                           unsigned OpNo) {
  assert(OpNo < Desc.NumOperands);
  unsigned RCID = Desc.OpInfo[OpNo].RegClass;
  return getRegBitWidth(MRI->getRegClass(RCID)) / 8;
}

bool isInlinableLiteral64(int64_t Literal, bool HasInv2Pi) {
  if (Literal >= -16 && Literal <= 64)
    return true;

  uint64_t Val = static_cast<uint64_t>(Literal);
  return (Val == DoubleToBits(0.0)) ||
         (Val == DoubleToBits(1.0)) ||
         (Val == DoubleToBits(-1.0)) ||
         (Val == DoubleToBits(0.5)) ||
         (Val == DoubleToBits(-0.5)) ||
         (Val == DoubleToBits(2.0)) ||
         (Val == DoubleToBits(-2.0)) ||
         (Val == DoubleToBits(4.0)) ||
         (Val == DoubleToBits(-4.0)) ||
         (Val == 0x3fc45f306dc9c882 && HasInv2Pi);
}

bool isInlinableLiteral32(int32_t Literal, bool HasInv2Pi) {
  if (Literal >= -16 && Literal <= 64)
    return true;

  // The actual type of the operand does not seem to matter as long
  // as the bits match one of the inline immediate values.  For example:
  //
  // -nan has the hexadecimal encoding of 0xfffffffe which is -2 in decimal,
  // so it is a legal inline immediate.
  //
  // 1065353216 has the hexadecimal encoding 0x3f800000 which is 1.0f in
  // floating-point, so it is a legal inline immediate.

  uint32_t Val = static_cast<uint32_t>(Literal);
  return (Val == FloatToBits(0.0f)) ||
         (Val == FloatToBits(1.0f)) ||
         (Val == FloatToBits(-1.0f)) ||
         (Val == FloatToBits(0.5f)) ||
         (Val == FloatToBits(-0.5f)) ||
         (Val == FloatToBits(2.0f)) ||
         (Val == FloatToBits(-2.0f)) ||
         (Val == FloatToBits(4.0f)) ||
         (Val == FloatToBits(-4.0f)) ||
         (Val == 0x3e22f983 && HasInv2Pi);
}

bool isInlinableLiteral16(int16_t Literal, bool HasInv2Pi) {
  if (!HasInv2Pi)
    return false;

  if (Literal >= -16 && Literal <= 64)
    return true;

  uint16_t Val = static_cast<uint16_t>(Literal);
  return Val == 0x3C00 || // 1.0
         Val == 0xBC00 || // -1.0
         Val == 0x3800 || // 0.5
         Val == 0xB800 || // -0.5
         Val == 0x4000 || // 2.0
         Val == 0xC000 || // -2.0
         Val == 0x4400 || // 4.0
         Val == 0xC400 || // -4.0
         Val == 0x3118;   // 1/2pi
}

bool isInlinableLiteralV216(int32_t Literal, bool HasInv2Pi) {
  assert(HasInv2Pi);

  if (!EnablePackedInlinableLiterals)
    return false;

  int16_t Lo16 = static_cast<int16_t>(Literal);
  int16_t Hi16 = static_cast<int16_t>(Literal >> 16);
  return Lo16 == Hi16 && isInlinableLiteral16(Lo16, HasInv2Pi);
}

bool isArgPassedInSGPR(const Argument *A) {
  const Function *F = A->getParent();

  // Arguments to compute shaders are never a source of divergence.
  CallingConv::ID CC = F->getCallingConv();
  switch (CC) {
  case CallingConv::AMDGPU_KERNEL:
  case CallingConv::SPIR_KERNEL:
    return true;
  case CallingConv::AMDGPU_VS:
  case CallingConv::AMDGPU_LS:
  case CallingConv::AMDGPU_HS:
  case CallingConv::AMDGPU_ES:
  case CallingConv::AMDGPU_GS:
  case CallingConv::AMDGPU_PS:
  case CallingConv::AMDGPU_CS:
    // For non-compute shaders, SGPR inputs are marked with either inreg or byval.
    // Everything else is in VGPRs.
    return F->getAttributes().hasParamAttribute(A->getArgNo(), Attribute::InReg) ||
           F->getAttributes().hasParamAttribute(A->getArgNo(), Attribute::ByVal);
  default:
    // TODO: Should calls support inreg for SGPR inputs?
    return false;
  }
}

int64_t getSMRDEncodedOffset(const MCSubtargetInfo &ST, int64_t ByteOffset) {
  if (isGCN3Encoding(ST))
    return ByteOffset;
  return ByteOffset >> 2;
}

bool isLegalSMRDImmOffset(const MCSubtargetInfo &ST, int64_t ByteOffset) {
  int64_t EncodedOffset = getSMRDEncodedOffset(ST, ByteOffset);
  return isGCN3Encoding(ST) ?
    isUInt<20>(EncodedOffset) : isUInt<8>(EncodedOffset);
}

} // end namespace AMDGPU

} // end namespace llvm

namespace llvm {
namespace AMDGPU {

AMDGPUAS getAMDGPUAS(Triple T) {
  AMDGPUAS AS;
  AS.FLAT_ADDRESS = 0;
  AS.PRIVATE_ADDRESS = 5;
  AS.REGION_ADDRESS = 4;
  return AS;
}

AMDGPUAS getAMDGPUAS(const TargetMachine &M) {
  return getAMDGPUAS(M.getTargetTriple());
}

AMDGPUAS getAMDGPUAS(const Module &M) {
  return getAMDGPUAS(Triple(M.getTargetTriple()));
}
} // namespace AMDGPU
} // namespace llvm
