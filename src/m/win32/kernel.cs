using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Text;

#if UCFG_CF
using SafeFileHandle = System.IntPtr;
#else
using Microsoft.Win32.SafeHandles;
#endif

using BYTE = System.Byte;
using SHORT = System.Int16;
using WORD = System.UInt16;
using DWORD = System.UInt32;
using LONG = System.Int32;
using HANDLE = System.IntPtr;

namespace Win32
{

public enum ImageType : uint
{
    IMAGE_DOS_SIGNATURE    = 0x5A4D,      // MZ
    IMAGE_OS2_SIGNATURE    = 0x454E,      // NE
    IMAGE_OS2_SIGNATURE_LE = 0x454C,      // LE
    IMAGE_VXD_SIGNATURE    = 0x454C,      // LE
    IMAGE_NT_SIGNATURE     =0x00004550  // PE00
}

public enum FileType : int {
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_DISK    = 0x0001,
    FILE_TYPE_CHAR    = 0x0002,
    FILE_TYPE_PIPE    = 0x0003,
    FILE_TYPE_REMOTE  = 0x8000
}

public enum StdHandleNumber : int
{
    STD_ERROR_HANDLE = -12,
    STD_OUTPUT_HANDLE = -11,
    STD_INPUT_HANDLE = -10
}

public enum CtrlEvent : uint
{
    CTRL_C_EVENT = 0,
    CTRL_BREAK_EVENT = 1
}


public struct IMAGE_DOS_HEADER
{      // DOS .EXE header
  public WORD   e_magic;                     // Magic number
    public WORD e_cblp;                      // Bytes on last page of file
    public WORD e_cp;                        // Pages in file
    public WORD e_crlc;                      // Relocations
    public WORD e_cparhdr;                   // Size of header in paragraphs
    public WORD e_minalloc;                  // Minimum extra paragraphs needed
    public WORD e_maxalloc;                  // Maximum extra paragraphs needed
    public WORD e_ss;                        // Initial (relative) SS value
    public WORD e_sp;                        // Initial SP value
    public WORD e_csum;                      // Checksum
    public WORD e_ip;                        // Initial IP value
    public WORD e_cs;                        // Initial (relative) CS value
    public WORD e_lfarlc;                    // File address of relocation table
    public WORD e_ovno;                      // Overlay number
    public WORD e_res_0, e_res_1, e_res_2, e_res_3;                    // Reserved words
    public WORD e_oemid;                     // OEM identifier (for e_oeminfo)
    public WORD e_oeminfo;                   // OEM information; e_oemid specific
  public WORD  e_res2_0, e_res2_1, e_res2_2, e_res2_3, e_res2_4, e_res2_5, e_res2_6, e_res2_7, e_res2_8, e_res2_90;                  // Reserved words
    public LONG e_lfanew;                    // File address of new exe header

    public static IMAGE_DOS_HEADER LoadFrom(BinaryReader r)
    {
        byte[] dosAr = new byte[Marshal.SizeOf(typeof(IMAGE_DOS_HEADER))];
        r.Read(dosAr, 0, dosAr.Length);
        unsafe {
            fixed (byte* dosb = dosAr)
                return *(IMAGE_DOS_HEADER*)dosb;
        }
    }
};

public enum MachineType : ushort
{
    IMAGE_FILE_MACHINE_UNKNOWN          = 0,
    IMAGE_FILE_MACHINE_I386             = 0x014c,  // Intel 386.
    IMAGE_FILE_MACHINE_R3000            = 0x0162,  // MIPS little-endian, 0x160 big-endian
    IMAGE_FILE_MACHINE_R4000            = 0x0166,  // MIPS little-endian
    IMAGE_FILE_MACHINE_R10000           = 0x0168,  // MIPS little-endian
    IMAGE_FILE_MACHINE_WCEMIPSV2        = 0x0169,  // MIPS little-endian WCE v2
    IMAGE_FILE_MACHINE_ALPHA            = 0x0184,  // Alpha_AXP
    IMAGE_FILE_MACHINE_SH3              = 0x01a2,  // SH3 little-endian
    IMAGE_FILE_MACHINE_SH3DSP           = 0x01a3,
    IMAGE_FILE_MACHINE_SH3E             = 0x01a4,  // SH3E little-endian
    IMAGE_FILE_MACHINE_SH4              = 0x01a6,  // SH4 little-endian
    IMAGE_FILE_MACHINE_SH5              = 0x01a8,  // SH5
    IMAGE_FILE_MACHINE_ARM              = 0x01c0,  // ARM Little-Endian
    IMAGE_FILE_MACHINE_THUMB            = 0x01c2,
    IMAGE_FILE_MACHINE_AM33             = 0x01d3,
    IMAGE_FILE_MACHINE_POWERPC          = 0x01F0,  // IBM PowerPC Little-Endian
    IMAGE_FILE_MACHINE_POWERPCFP        = 0x01f1,
    IMAGE_FILE_MACHINE_IA64             = 0x0200,  // Intel 64
    IMAGE_FILE_MACHINE_MIPS16           = 0x0266,  // MIPS
    IMAGE_FILE_MACHINE_ALPHA64          = 0x0284,  // ALPHA64
    IMAGE_FILE_MACHINE_MIPSFPU          = 0x0366,  // MIPS
    IMAGE_FILE_MACHINE_MIPSFPU16        = 0x0466,  // MIPS
    IMAGE_FILE_MACHINE_AXP64            = IMAGE_FILE_MACHINE_ALPHA64,
    IMAGE_FILE_MACHINE_TRICORE          = 0x0520,  // Infineon
    IMAGE_FILE_MACHINE_CEF              = 0x0CEF,
    IMAGE_FILE_MACHINE_EBC              = 0x0EBC,  // EFI Byte Code
    IMAGE_FILE_MACHINE_AMD64            = 0x8664,  // AMD64 (K8)
    IMAGE_FILE_MACHINE_M32R             = 0x9041,  // M32R little-endian
    IMAGE_FILE_MACHINE_CEE              = 0xC0EE
}


public struct IMAGE_FILE_HEADER
{
  public MachineType    Machine;
  public WORD    NumberOfSections;
  public DWORD   TimeDateStamp;
  public DWORD   PointerToSymbolTable;
  public DWORD   NumberOfSymbols;
  public WORD    SizeOfOptionalHeader;
  public WORD    Characteristics;

    public static IMAGE_FILE_HEADER LoadFrom(BinaryReader r)
    {
        IMAGE_FILE_HEADER h = new IMAGE_FILE_HEADER();
        h.Machine = (MachineType)r.ReadUInt16();
        h.NumberOfSections = r.ReadUInt16();
        h.TimeDateStamp = r.ReadUInt32();
        h.PointerToSymbolTable = r.ReadUInt32();
        h.NumberOfSymbols = r.ReadUInt32();
        h.SizeOfOptionalHeader = r.ReadUInt16();
        h.Characteristics = r.ReadUInt16();
        return h;
    }

};

public enum ImageDirectoryIndex
{
    IMAGE_DIRECTORY_ENTRY_EXPORT          = 0,   // Export Directory
    IMAGE_DIRECTORY_ENTRY_IMPORT          = 1,   // Import Directory
    IMAGE_DIRECTORY_ENTRY_RESOURCE        = 2,   // Resource Directory
    IMAGE_DIRECTORY_ENTRY_EXCEPTION       = 3,   // Exception Directory
    IMAGE_DIRECTORY_ENTRY_SECURITY        = 4,   // Security Directory
    IMAGE_DIRECTORY_ENTRY_BASERELOC       = 5,   // Base Relocation Table
    IMAGE_DIRECTORY_ENTRY_DEBUG           = 6,   // Debug Directory
    IMAGE_DIRECTORY_ENTRY_COPYRIGHT       = 7,   // (X86 usage)
    IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    = 7,   // Architecture Specific Data
    IMAGE_DIRECTORY_ENTRY_GLOBALPTR       = 8,   // RVA of GP
    IMAGE_DIRECTORY_ENTRY_TLS             = 9,   // TLS Directory
    IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    = 10,   // Load Configuration Directory
    IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   = 11,   // Bound Import Directory in headers
    IMAGE_DIRECTORY_ENTRY_IAT            = 12,   // Import Address Table
    IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   = 13,   // Delay Load Import Descriptors
    IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR = 14   // COM Runtime descriptor
};

public struct IMAGE_DATA_DIRECTORY
{
  public DWORD   VirtualAddress;
    public DWORD Size;
}

public struct IMAGE_OPTIONAL_HEADER
{
    public WORD Magic;
    public BYTE MajorLinkerVersion;
    public BYTE MinorLinkerVersion;
    public DWORD SizeOfCode;
    public DWORD SizeOfInitializedData;
    public DWORD SizeOfUninitializedData;
    public DWORD AddressOfEntryPoint;
  public DWORD   BaseOfCode;
  public DWORD   BaseOfData;
    public DWORD ImageBase;
    public DWORD SectionAlignment;
    public DWORD FileAlignment;
    public WORD MajorOperatingSystemVersion;
    public WORD MinorOperatingSystemVersion;
    public WORD MajorImageVersion;
    public WORD MinorImageVersion;
    public WORD MajorSubsystemVersion;
    public WORD MinorSubsystemVersion;
    public DWORD Win32VersionValue;
    public DWORD SizeOfImage;
    public DWORD SizeOfHeaders;
    public DWORD CheckSum;
    public WORD Subsystem;
    public WORD DllCharacteristics;
    public DWORD SizeOfStackReserve;
    public DWORD SizeOfStackCommit;
    public DWORD SizeOfHeapReserve;
    public DWORD SizeOfHeapCommit;
    public DWORD LoaderFlags;
    public DWORD NumberOfRvaAndSizes;
  // IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];

    public static IMAGE_OPTIONAL_HEADER LoadFrom(BinaryReader r)
    {
        byte[] dosAr = new byte[Marshal.SizeOf(typeof(IMAGE_OPTIONAL_HEADER))];
        r.Read(dosAr, 0, dosAr.Length);
        unsafe {
            fixed (byte* dosb = dosAr)
                return *(IMAGE_OPTIONAL_HEADER*)dosb;
        }
    }
}

public struct IMAGE_IMPORT_DESCRIPTOR
{
    public DWORD OriginalFirstThunk;
    public DWORD TimeDateStamp;
  public DWORD ForwarderChain;
  public DWORD Name;
  public DWORD FirstThunk;

    public static IMAGE_IMPORT_DESCRIPTOR LoadFrom(BinaryReader r)
    {
        IMAGE_IMPORT_DESCRIPTOR h = new IMAGE_IMPORT_DESCRIPTOR();
        h.OriginalFirstThunk = r.ReadUInt32();
        h.TimeDateStamp = r.ReadUInt32();
        h.ForwarderChain = r.ReadUInt32();
        h.Name = r.ReadUInt32();
        h.FirstThunk = r.ReadUInt32();
        return h;
    }
}

[Flags]
public enum SectionCharacteristics : uint
{
    IMAGE_SCN_TYPE_NO_PAD             =   0x00000008,  // Reserved.
    IMAGE_SCN_TYPE_COPY               =   0x00000010,  // Reserved.
    IMAGE_SCN_CNT_CODE                =   0x00000020,  // Section contains code.
    IMAGE_SCN_CNT_INITIALIZED_DATA    =   0x00000040,  // Section contains initialized data.
    IMAGE_SCN_CNT_UNINITIALIZED_DATA  =   0x00000080,  // Section contains uninitialized data.
    IMAGE_SCN_LNK_OTHER               =   0x00000100,  // Reserved.
    IMAGE_SCN_LNK_INFO                =   0x00000200,  // Section contains comments or some other type of information.
    IMAGE_SCN_TYPE_OVER               =   0x00000400,  // Reserved.
    IMAGE_SCN_LNK_REMOVE              =   0x00000800,  // Section contents will not become part of image.
    IMAGE_SCN_LNK_COMDAT              =   0x00001000,  // Section contents comdat.
    IMAGE_SCN_NO_DEFER_SPEC_EXC       =   0x00004000,  // Reset speculative exceptions handling bits in the TLB entries for this section.
    IMAGE_SCN_GPREL                   =   0x00008000,  // Section content can be accessed relative to GP
    IMAGE_SCN_MEM_FARDATA             =   0x00008000,
    IMAGE_SCN_MEM_PURGEABLE           =   0x00020000,
    IMAGE_SCN_MEM_16BIT               =   0x00020000,
    IMAGE_SCN_MEM_LOCKED              =   0x00040000,
    IMAGE_SCN_MEM_PRELOAD             =   0x00080000,
    IMAGE_SCN_ALIGN_1BYTES            =  0x00100000,  //
    IMAGE_SCN_ALIGN_2BYTES            =   0x00200000,  //
    IMAGE_SCN_ALIGN_4BYTES            =   0x00300000,  //
    IMAGE_SCN_ALIGN_8BYTES            =   0x00400000,  //
    IMAGE_SCN_ALIGN_16BYTES           =   0x00500000,  // Default alignment if no others are specified.
    IMAGE_SCN_ALIGN_32BYTES           =   0x00600000,  //
    IMAGE_SCN_ALIGN_64BYTES           =   0x00700000,  //
    IMAGE_SCN_ALIGN_128BYTES          =   0x00800000,  //
    IMAGE_SCN_ALIGN_256BYTES          =   0x00900000,  //
    IMAGE_SCN_ALIGN_512BYTES          =   0x00A00000,  //
    IMAGE_SCN_ALIGN_1024BYTES         =   0x00B00000,  //
    IMAGE_SCN_ALIGN_2048BYTES         =   0x00C00000,  //
    IMAGE_SCN_ALIGN_4096BYTES         =   0x00D00000,  //
    IMAGE_SCN_ALIGN_8192BYTES         =   0x00E00000,  //
    IMAGE_SCN_ALIGN_MASK              =   0x00F00000,
    IMAGE_SCN_LNK_NRELOC_OVFL         =   0x01000000,  // Section contains extended relocations.
    IMAGE_SCN_MEM_DISCARDABLE         =   0x02000000,  // Section can be discarded.
    IMAGE_SCN_MEM_NOT_CACHED          =   0x04000000,  // Section is not cachable.
    IMAGE_SCN_MEM_NOT_PAGED           =   0x08000000,  // Section is not pageable.
    IMAGE_SCN_MEM_SHARED              =   0x10000000,  // Section is shareable.
    IMAGE_SCN_MEM_EXECUTE             =   0x20000000,  // Section is executable.
    IMAGE_SCN_MEM_READ                =   0x40000000,  // Section is readable.
    IMAGE_SCN_MEM_WRITE               =   0x80000000  // Section is writeable.
}

public struct IMAGE_SECTION_HEADER
{
    public const int IMAGE_SIZEOF_SHORT_NAME = 8;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = IMAGE_SIZEOF_SHORT_NAME)]
  public BYTE[]    Name;
  public DWORD   VirtualSize; // unioun with PhysicalAddress;
  public DWORD   VirtualAddress;
  public DWORD   SizeOfRawData;
  public DWORD   PointerToRawData;
  public DWORD   PointerToRelocations;
  public DWORD   PointerToLinenumbers;
  public WORD    NumberOfRelocations;
  public WORD    NumberOfLinenumbers;
  public SectionCharacteristics   Characteristics;

    public static IMAGE_SECTION_HEADER LoadFrom(BinaryReader r)
    {
        IMAGE_SECTION_HEADER h = new IMAGE_SECTION_HEADER();
        h.Name = r.ReadBytes(IMAGE_SIZEOF_SHORT_NAME);
        h.VirtualSize = r.ReadUInt32();
        h.VirtualAddress = r.ReadUInt32();
        h.SizeOfRawData = r.ReadUInt32();
        h.PointerToRawData = r.ReadUInt32();
        h.PointerToRelocations = r.ReadUInt32();
        h.PointerToLinenumbers = r.ReadUInt32();
        h.NumberOfRelocations = r.ReadUInt16();
        h.PointerToLinenumbers = r.ReadUInt16();
        h.Characteristics = (SectionCharacteristics)r.ReadUInt32();
        return h;
    }
}

public enum SymType : ushort
{
    IMAGE_SYM_TYPE_NULL        =         0x0000,  // no type.
    IMAGE_SYM_TYPE_VOID        =         0x0001,  //
    IMAGE_SYM_TYPE_CHAR        =         0x0002,  // type character.
    IMAGE_SYM_TYPE_SHORT       =         0x0003,  // type short integer.
    IMAGE_SYM_TYPE_INT         =         0x0004,  //
    IMAGE_SYM_TYPE_LONG        =         0x0005,  //
    IMAGE_SYM_TYPE_FLOAT       =         0x0006,  //
    IMAGE_SYM_TYPE_DOUBLE      =         0x0007,  //
    IMAGE_SYM_TYPE_STRUCT      =         0x0008,  //
    IMAGE_SYM_TYPE_UNION       =         0x0009,  //
    IMAGE_SYM_TYPE_ENUM        =         0x000A,  // enumeration.
    IMAGE_SYM_TYPE_MOE         =         0x000B,  // member of enumeration.
    IMAGE_SYM_TYPE_BYTE        =         0x000C,  //
    IMAGE_SYM_TYPE_WORD        =         0x000D,  //
    IMAGE_SYM_TYPE_UINT        =         0x000E,  //
    IMAGE_SYM_TYPE_DWORD       =         0x000F,  //
    IMAGE_SYM_TYPE_PCODE       =         0x8000  //
}

public enum SymStorageClass : byte
{
    IMAGE_SYM_CLASS_END_OF_FUNCTION   =  255,
    IMAGE_SYM_CLASS_NULL              =  0x0000,
    IMAGE_SYM_CLASS_AUTOMATIC         =  0x0001,
    IMAGE_SYM_CLASS_EXTERNAL          =  0x0002,
    IMAGE_SYM_CLASS_STATIC            =  0x0003,
    IMAGE_SYM_CLASS_REGISTER          =  0x0004,
    IMAGE_SYM_CLASS_EXTERNAL_DEF      =  0x0005,
    IMAGE_SYM_CLASS_LABEL             =  0x0006,
    IMAGE_SYM_CLASS_UNDEFINED_LABEL   =  0x0007,
    IMAGE_SYM_CLASS_MEMBER_OF_STRUCT  =  0x0008,
    IMAGE_SYM_CLASS_ARGUMENT          =  0x0009,
    IMAGE_SYM_CLASS_STRUCT_TAG        =  0x000A,
    IMAGE_SYM_CLASS_MEMBER_OF_UNION   =  0x000B,
    IMAGE_SYM_CLASS_UNION_TAG         =  0x000C,
    IMAGE_SYM_CLASS_TYPE_DEFINITION   =  0x000D,
    IMAGE_SYM_CLASS_UNDEFINED_STATIC  =  0x000E,
    IMAGE_SYM_CLASS_ENUM_TAG          =  0x000F,
    IMAGE_SYM_CLASS_MEMBER_OF_ENUM    =  0x0010,
    IMAGE_SYM_CLASS_REGISTER_PARAM    =  0x0011,
    IMAGE_SYM_CLASS_BIT_FIELD         =  0x0012,
    IMAGE_SYM_CLASS_FAR_EXTERNAL      =  0x0044,  //
    IMAGE_SYM_CLASS_BLOCK             =  0x0064,
    IMAGE_SYM_CLASS_FUNCTION          =  0x0065,
    IMAGE_SYM_CLASS_END_OF_STRUCT     =  0x0066,
    IMAGE_SYM_CLASS_FILE              =  0x0067,
    IMAGE_SYM_CLASS_SECTION           =  0x0068,
    IMAGE_SYM_CLASS_WEAK_EXTERNAL     =  0x0069,
    IMAGE_SYM_CLASS_CLR_TOKEN         =  0x006B
}

[StructLayout(LayoutKind.Sequential
#if !UCFG_CF
    ,Pack=1
#endif
    )]
public struct IMAGE_SYMBOL
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
    public BYTE[] ShortName;

    public DWORD LongName0 {
        get { return (uint)(ShortName[0] | (ShortName[1]<<8) | (ShortName[2]<<16) | (ShortName[3]<<24)); }
    }

    public DWORD LongName1 {
        get { return (uint)(ShortName[4] | (ShortName[5]<<8) | (ShortName[6]<<16) | (ShortName[7]<<24)); }
    }

    public DWORD Value;
  public SHORT SectionNumber;
  public SymType    Type;
  public   SymStorageClass    StorageClass;
  public  BYTE    NumberOfAuxSymbols;

    public static IMAGE_SYMBOL LoadFrom(BinaryReader r)
    {
        IMAGE_SYMBOL h = new IMAGE_SYMBOL();
        h.ShortName = r.ReadBytes(8);
        h.Value = r.ReadUInt32();
        h.SectionNumber = r.ReadInt16();
        h.Type = (SymType)r.ReadUInt16();
        h.StorageClass = (SymStorageClass)r.ReadByte();
        h.NumberOfAuxSymbols = r.ReadByte();
        return h;
    }
}

public struct IMAGE_ARCHIVE_MEMBER_HEADER
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
    BYTE[]     Name;                          // File member name - `/' terminated.

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 12)]
  BYTE[]     Date;                          // File member date - decimal.

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
  BYTE[]     UserID;                         // File member user id - decimal.

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
  BYTE[]     GroupID;                        // File member group id - decimal.

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
  BYTE[]     Mode;                           // File member mode - octal.

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 10)]
  BYTE[]     Size;                          // File member size - decimal.

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
  BYTE[]     EndHeader;                      // String to end header.

    public static IMAGE_ARCHIVE_MEMBER_HEADER LoadFrom(BinaryReader r)
    {
        IMAGE_ARCHIVE_MEMBER_HEADER h = new IMAGE_ARCHIVE_MEMBER_HEADER();
        h.Name = r.ReadBytes(h.Name.Length);
        h.Date = r.ReadBytes(h.Date.Length);
        h.UserID = r.ReadBytes(h.UserID.Length);
        h.GroupID = r.ReadBytes(h.GroupID.Length);
        h.Mode = r.ReadBytes(h.Mode.Length);
        h.Size = r.ReadBytes(h.Size.Length);
        h.EndHeader = r.ReadBytes(h.EndHeader.Length);
        return h;
    }
}

public enum IMPORT_OBJECT_NAME_TYPE
{
    IMPORT_OBJECT_ORDINAL = 0,          // Import by ordinal
  IMPORT_OBJECT_NAME = 1,             // Import name == public symbol name.
  IMPORT_OBJECT_NAME_NO_PREFIX = 2,   // Import name == public symbol name skipping leading ?, @, or optionally _.
  IMPORT_OBJECT_NAME_UNDECORATE = 3,  // Import name == public symbol name skipping leading ?, @, or optionally _ and truncating at first @
}

public struct IMPORT_OBJECT_HEADER
{
  public WORD    Sig1;                       // Must be IMAGE_FILE_MACHINE_UNKNOWN
  public WORD    Sig2;                       // Must be IMPORT_OBJECT_HDR_SIG2.
  public WORD    Version;
  public MachineType    Machine;
  public DWORD   TimeDateStamp;              // Time/date stamp
  public DWORD   SizeOfData;                 // particularly useful for incremental links

  public WORD    Ordinal;                // if grf & IMPORT_OBJECT_ORDINAL

    public WORD    Hint => Ordinal;

  //public WORD    Type : 2;                   // IMPORT_TYPE
  //public WORD    NameType : 3;               // IMPORT_NAME_TYPE
  //public WORD    Reserved : 11;              // Reserved. Must be zero.
    public WORD Fields;

    public IMPORT_OBJECT_NAME_TYPE NameType {
        get { return (IMPORT_OBJECT_NAME_TYPE)((Fields>>2)&7); }
    }

    public static IMPORT_OBJECT_HEADER LoadFrom(BinaryReader r)
    {
        byte[] dosAr = new byte[Marshal.SizeOf(typeof(IMPORT_OBJECT_HEADER))];
        r.Read(dosAr, 0, dosAr.Length);
        unsafe {
            fixed (byte* dosb = dosAr)
                return *(IMPORT_OBJECT_HEADER*)dosb;
        }
    }
}

public enum RelType : ushort
{
    IMAGE_REL_I386_ABSOLUTE      =   0x0000,  // Reference is absolute, no relocation is necessary
    IMAGE_REL_I386_DIR16         =   0x0001,  // Direct 16-bit reference to the symbols virtual address
    IMAGE_REL_I386_REL16         =   0x0002,  // PC-relative 16-bit reference to the symbols virtual address
    IMAGE_REL_I386_DIR32         =   0x0006,  // Direct 32-bit reference to the symbols virtual address
    IMAGE_REL_I386_DIR32NB       =   0x0007,  // Direct 32-bit reference to the symbols virtual address, base not included
    IMAGE_REL_I386_SEG12         =   0x0009,  // Direct 16-bit reference to the segment-selector bits of a 32-bit virtual address
    IMAGE_REL_I386_SECTION       =   0x000A,
    IMAGE_REL_I386_SECREL        =   0x000B,
    IMAGE_REL_I386_TOKEN         =   0x000C,  // clr token
    IMAGE_REL_I386_SECREL7       =   0x000D,  // 7 bit offset from base of section containing target
    IMAGE_REL_I386_REL32         =   0x0014,  // PC-relative 32-bit reference to the symbols virtual address

    IMAGE_REL_MIPS_ABSOLUTE      =   0x0000,  // Reference is absolute, no relocation is necessary
    IMAGE_REL_MIPS_REFHALF       =   0x0001,
    IMAGE_REL_MIPS_REFWORD       =   0x0002,
    IMAGE_REL_MIPS_JMPADDR       =   0x0003,
    IMAGE_REL_MIPS_REFHI         =   0x0004,
    IMAGE_REL_MIPS_REFLO         =   0x0005,
    IMAGE_REL_MIPS_GPREL         =   0x0006,
    IMAGE_REL_MIPS_LITERAL       =   0x0007,
    IMAGE_REL_MIPS_SECTION       =   0x000A,
    IMAGE_REL_MIPS_SECREL        =   0x000B,
    IMAGE_REL_MIPS_SECRELLO      =   0x000C,  // Low 16-bit section relative referemce (used for >32k TLS)
    IMAGE_REL_MIPS_SECRELHI      =   0x000D,  // High 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_MIPS_TOKEN         =   0x000E,  // clr token
    IMAGE_REL_MIPS_JMPADDR16     =   0x0010,
    IMAGE_REL_MIPS_REFWORDNB     =   0x0022,
    IMAGE_REL_MIPS_PAIR          =   0x0025,

    IMAGE_REL_ALPHA_ABSOLUTE     =   0x0000,
    IMAGE_REL_ALPHA_REFLONG      =   0x0001,
    IMAGE_REL_ALPHA_REFQUAD       =  0x0002,
    IMAGE_REL_ALPHA_GPREL32       =  0x0003,
    IMAGE_REL_ALPHA_LITERAL       =  0x0004,
    IMAGE_REL_ALPHA_LITUSE        =  0x0005,
    IMAGE_REL_ALPHA_GPDISP        =  0x0006,
    IMAGE_REL_ALPHA_BRADDR        =  0x0007,
    IMAGE_REL_ALPHA_HINT          =  0x0008,
    IMAGE_REL_ALPHA_INLINE_REFLONG=  0x0009,
    IMAGE_REL_ALPHA_REFHI         =  0x000A,
    IMAGE_REL_ALPHA_REFLO         =  0x000B,
    IMAGE_REL_ALPHA_PAIR          =  0x000C,
    IMAGE_REL_ALPHA_MATCH         =  0x000D,
    IMAGE_REL_ALPHA_SECTION       =  0x000E,
    IMAGE_REL_ALPHA_SECREL        =  0x000F,
    IMAGE_REL_ALPHA_REFLONGNB     =  0x0010,
    IMAGE_REL_ALPHA_SECRELLO      =  0x0011,  // Low 16-bit section relative reference
    IMAGE_REL_ALPHA_SECRELHI      =  0x0012,  // High 16-bit section relative reference
    IMAGE_REL_ALPHA_REFQ3         =  0x0013,  // High 16 bits of 48 bit reference
    IMAGE_REL_ALPHA_REFQ2         =  0x0014,  // Middle 16 bits of 48 bit reference
    IMAGE_REL_ALPHA_REFQ1         =  0x0015,  // Low 16 bits of 48 bit reference
    IMAGE_REL_ALPHA_GPRELLO       =  0x0016,  // Low 16-bit GP relative reference
    IMAGE_REL_ALPHA_GPRELHI       =  0x0017,  // High 16-bit GP relative reference

    IMAGE_REL_PPC_ABSOLUTE        =  0x0000,  // NOP
    IMAGE_REL_PPC_ADDR64          =  0x0001,  // 64-bit address
    IMAGE_REL_PPC_ADDR32          =  0x0002,  // 32-bit address
    IMAGE_REL_PPC_ADDR24          =  0x0003,  // 26-bit address, shifted left 2 (branch absolute)
    IMAGE_REL_PPC_ADDR16          =  0x0004,  // 16-bit address
    IMAGE_REL_PPC_ADDR14          =  0x0005,  // 16-bit address, shifted left 2 (load doubleword)
    IMAGE_REL_PPC_REL24           =  0x0006,  // 26-bit PC-relative offset, shifted left 2 (branch relative)
    IMAGE_REL_PPC_REL14           =  0x0007,  // 16-bit PC-relative offset, shifted left 2 (br cond relative)
    IMAGE_REL_PPC_TOCREL16        =  0x0008,  // 16-bit offset from TOC base
    IMAGE_REL_PPC_TOCREL14        =  0x0009,  // 16-bit offset from TOC base, shifted left 2 (load doubleword)

    IMAGE_REL_PPC_ADDR32NB        =  0x000A,  // 32-bit addr w/o image base
    IMAGE_REL_PPC_SECREL          =  0x000B,  // va of containing section (as in an image sectionhdr)
    IMAGE_REL_PPC_SECTION         =  0x000C,  // sectionheader number
    IMAGE_REL_PPC_IFGLUE          =  0x000D,  // substitute TOC restore instruction iff symbol is glue code
    IMAGE_REL_PPC_IMGLUE          =  0x000E,  // symbol is glue code; virtual address is TOC restore instruction
    IMAGE_REL_PPC_SECREL16        =  0x000F,  // va of containing section (limited to 16 bits)
    IMAGE_REL_PPC_REFHI           =  0x0010,
    IMAGE_REL_PPC_REFLO           =  0x0011,
    IMAGE_REL_PPC_PAIR            =  0x0012,
    IMAGE_REL_PPC_SECRELLO        =  0x0013,  // Low 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_PPC_SECRELHI        =  0x0014,  // High 16-bit section relative reference (used for >32k TLS)
    IMAGE_REL_PPC_GPREL           =  0x0015,
    IMAGE_REL_PPC_TOKEN           =  0x0016,  // clr token

    IMAGE_REL_PPC_TYPEMASK        =  0x00FF,  // mask to isolate above values in IMAGE_RELOCATION.Type

    IMAGE_REL_PPC_NEG             =  0x0100,  // subtract reloc value rather than adding it
    IMAGE_REL_PPC_BRTAKEN         =  0x0200,  // fix branch prediction bit to predict branch taken
    IMAGE_REL_PPC_BRNTAKEN        =  0x0400,  // fix branch prediction bit to predict branch not taken
    IMAGE_REL_PPC_TOCDEFN         =  0x0800,  // toc slot defined in file (or, data in toc)

    IMAGE_REL_SH3_ABSOLUTE        =  0x0000,  // No relocation
    IMAGE_REL_SH3_DIRECT16        =  0x0001,  // 16 bit direct
    IMAGE_REL_SH3_DIRECT32        =  0x0002,  // 32 bit direct
    IMAGE_REL_SH3_DIRECT8         =  0x0003,  // 8 bit direct, -128..255
    IMAGE_REL_SH3_DIRECT8_WORD    =  0x0004,  // 8 bit direct .W (0 ext.)
    IMAGE_REL_SH3_DIRECT8_LONG    =  0x0005,  // 8 bit direct .L (0 ext.)
    IMAGE_REL_SH3_DIRECT4         =  0x0006,  // 4 bit direct (0 ext.)
    IMAGE_REL_SH3_DIRECT4_WORD    =  0x0007,  // 4 bit direct .W (0 ext.)
    IMAGE_REL_SH3_DIRECT4_LONG    =  0x0008,  // 4 bit direct .L (0 ext.)
    IMAGE_REL_SH3_PCREL8_WORD     =  0x0009,  // 8 bit PC relative .W
    IMAGE_REL_SH3_PCREL8_LONG     =  0x000A,  // 8 bit PC relative .L
    IMAGE_REL_SH3_PCREL12_WORD    =  0x000B,  // 12 LSB PC relative .W
    IMAGE_REL_SH3_STARTOF_SECTION =  0x000C,  // Start of EXE section
    IMAGE_REL_SH3_SIZEOF_SECTION  =  0x000D,  // Size of EXE section
    IMAGE_REL_SH3_SECTION         =  0x000E,  // Section table index
    IMAGE_REL_SH3_SECREL          =  0x000F,  // Offset within section
    IMAGE_REL_SH3_DIRECT32_NB     =  0x0010,  // 32 bit direct not based
    IMAGE_REL_SH3_GPREL4_LONG     =  0x0011,  // GP-relative addressing
    IMAGE_REL_SH3_TOKEN           =  0x0012,  // clr token

    IMAGE_REL_ARM_ABSOLUTE        =  0x0000,  // No relocation required
    IMAGE_REL_ARM_ADDR32          =  0x0001,  // 32 bit address
    IMAGE_REL_ARM_ADDR32NB        =  0x0002,  // 32 bit address w/o image base
    IMAGE_REL_ARM_BRANCH24        =  0x0003,  // 24 bit offset << 2 & sign ext.
    IMAGE_REL_ARM_BRANCH11        =  0x0004,  // Thumb: 2 11 bit offsets
    IMAGE_REL_ARM_TOKEN           =  0x0005,  // clr token
    IMAGE_REL_ARM_GPREL12         =  0x0006,  // GP-relative addressing (ARM)
    IMAGE_REL_ARM_GPREL7          =  0x0007,  // GP-relative addressing (Thumb)
    IMAGE_REL_ARM_BLX24           =  0x0008,
    IMAGE_REL_ARM_BLX11           =  0x0009,
    IMAGE_REL_ARM_SECTION         =  0x000E,  // Section table index
    IMAGE_REL_ARM_SECREL          =  0x000F,  // Offset within section

    IMAGE_REL_AM_ABSOLUTE         =  0x0000,
    IMAGE_REL_AM_ADDR32           =  0x0001,
    IMAGE_REL_AM_ADDR32NB         =  0x0002,
    IMAGE_REL_AM_CALL32           =  0x0003,
    IMAGE_REL_AM_FUNCINFO         =  0x0004,
    IMAGE_REL_AM_REL32_1          =  0x0005,
    IMAGE_REL_AM_REL32_2          =  0x0006,
    IMAGE_REL_AM_SECREL           =  0x0007,
    IMAGE_REL_AM_SECTION          =  0x0008,
    IMAGE_REL_AM_TOKEN            =  0x0009,

    IMAGE_REL_AMD64_ABSOLUTE      =  0x0000,  // Reference is absolute, no relocation is necessary
    IMAGE_REL_AMD64_ADDR64        =  0x0001,  // 64-bit address (VA).
    IMAGE_REL_AMD64_ADDR32        =  0x0002,  // 32-bit address (VA).
    IMAGE_REL_AMD64_ADDR32NB      =  0x0003,  // 32-bit address w/o image base (RVA).
    IMAGE_REL_AMD64_REL32         =  0x0004,  // 32-bit relative address from byte following reloc
    IMAGE_REL_AMD64_REL32_1       =  0x0005,  // 32-bit relative address from byte distance 1 from reloc
    IMAGE_REL_AMD64_REL32_2       =  0x0006,  // 32-bit relative address from byte distance 2 from reloc
    IMAGE_REL_AMD64_REL32_3       =  0x0007,  // 32-bit relative address from byte distance 3 from reloc
    IMAGE_REL_AMD64_REL32_4       =  0x0008,  // 32-bit relative address from byte distance 4 from reloc
    IMAGE_REL_AMD64_REL32_5       =  0x0009,  // 32-bit relative address from byte distance 5 from reloc
    IMAGE_REL_AMD64_SECTION       =  0x000A,  // Section index
    IMAGE_REL_AMD64_SECREL        =  0x000B,  // 32 bit offset from base of section containing target
    IMAGE_REL_AMD64_SECREL7       =  0x000C,  // 7 bit unsigned offset from base of section containing target
    IMAGE_REL_AMD64_TOKEN         =  0x000D,  // 32 bit metadata token
    IMAGE_REL_AMD64_SREL32        =  0x000E,  // 32 bit signed span-dependent value emitted into object
    IMAGE_REL_AMD64_PAIR          =  0x000F,
    IMAGE_REL_AMD64_SSPAN32       =  0x0010,  // 32 bit signed span-dependent value applied at link time

    IMAGE_REL_IA64_ABSOLUTE       =  0x0000,
    IMAGE_REL_IA64_IMM14          =  0x0001,
    IMAGE_REL_IA64_IMM22          =  0x0002,
    IMAGE_REL_IA64_IMM64          =  0x0003,
    IMAGE_REL_IA64_DIR32          =  0x0004,
    IMAGE_REL_IA64_DIR64          =  0x0005,
    IMAGE_REL_IA64_PCREL21B       =  0x0006,
    IMAGE_REL_IA64_PCREL21M       =  0x0007,
    IMAGE_REL_IA64_PCREL21F       =  0x0008,
    IMAGE_REL_IA64_GPREL22        =  0x0009,
    IMAGE_REL_IA64_LTOFF22        =  0x000A,
    IMAGE_REL_IA64_SECTION        =  0x000B,
    IMAGE_REL_IA64_SECREL22       =  0x000C,
    IMAGE_REL_IA64_SECREL64I      =  0x000D,
    IMAGE_REL_IA64_SECREL32       =  0x000E,

    IMAGE_REL_IA64_DIR32NB        =  0x0010,
    IMAGE_REL_IA64_SREL14         =  0x0011,
    IMAGE_REL_IA64_SREL22         =  0x0012,
    IMAGE_REL_IA64_SREL32         =  0x0013,
    IMAGE_REL_IA64_UREL32         =  0x0014,
    IMAGE_REL_IA64_PCREL60X       =  0x0015,  // This is always a BRL and never converted
    IMAGE_REL_IA64_PCREL60B       =  0x0016,  // If possible, convert to MBB bundle with NOP.B in slot 1
    IMAGE_REL_IA64_PCREL60F       =  0x0017,  // If possible, convert to MFB bundle with NOP.F in slot 1
    IMAGE_REL_IA64_PCREL60I       =  0x0018,  // If possible, convert to MIB bundle with NOP.I in slot 1
    IMAGE_REL_IA64_PCREL60M       =  0x0019,  // If possible, convert to MMB bundle with NOP.M in slot 1
    IMAGE_REL_IA64_IMMGPREL64     =  0x001A,
    IMAGE_REL_IA64_TOKEN          =  0x001B,  // clr token
    IMAGE_REL_IA64_GPREL32        =  0x001C,
    IMAGE_REL_IA64_ADDEND         =  0x001F,

    IMAGE_REL_CEF_ABSOLUTE        =  0x0000,  // Reference is absolute, no relocation is necessary
    IMAGE_REL_CEF_ADDR32          =  0x0001,  // 32-bit address (VA).
    IMAGE_REL_CEF_ADDR64          =  0x0002,  // 64-bit address (VA).
    IMAGE_REL_CEF_ADDR32NB        =  0x0003,  // 32-bit address w/o image base (RVA).
    IMAGE_REL_CEF_SECTION         =  0x0004,  // Section index
    IMAGE_REL_CEF_SECREL          =  0x0005,  // 32 bit offset from base of section containing target
    IMAGE_REL_CEF_TOKEN           =  0x0006,  // 32 bit metadata token

    IMAGE_REL_CEE_ABSOLUTE        =  0x0000,  // Reference is absolute, no relocation is necessary
    IMAGE_REL_CEE_ADDR32          =  0x0001,  // 32-bit address (VA).
    IMAGE_REL_CEE_ADDR64          =  0x0002,  // 64-bit address (VA).
    IMAGE_REL_CEE_ADDR32NB        =  0x0003,  // 32-bit address w/o image base (RVA).
    IMAGE_REL_CEE_SECTION         =  0x0004,  // Section index
    IMAGE_REL_CEE_SECREL      =      0x0005,  // 32 bit offset from base of section containing target
    IMAGE_REL_CEE_TOKEN       =      0x0006,  // 32 bit metadata token

    IMAGE_REL_M32R_ABSOLUTE   =    0x0000,   // No relocation required
    IMAGE_REL_M32R_ADDR32     =    0x0001,   // 32 bit address
    IMAGE_REL_M32R_ADDR32NB   =    0x0002,   // 32 bit address w/o image base
    IMAGE_REL_M32R_ADDR24     =    0x0003,   // 24 bit address
    IMAGE_REL_M32R_GPREL16    =    0x0004,   // GP relative addressing
    IMAGE_REL_M32R_PCREL24    =    0x0005,   // 24 bit offset << 2 & sign ext.
    IMAGE_REL_M32R_PCREL16    =    0x0006,   // 16 bit offset << 2 & sign ext.
    IMAGE_REL_M32R_PCREL8     =    0x0007,   // 8 bit offset << 2 & sign ext.
    IMAGE_REL_M32R_REFHALF    =    0x0008,   // 16 MSBs
    IMAGE_REL_M32R_REFHI      =    0x0009,   // 16 MSBs; adj for LSB sign ext.
    IMAGE_REL_M32R_REFLO      =    0x000A,   // 16 LSBs
    IMAGE_REL_M32R_PAIR       =    0x000B,   // Link HI and LO
    IMAGE_REL_M32R_SECTION    =    0x000C,   // Section table index
    IMAGE_REL_M32R_SECREL32   =    0x000D,   // 32 bit section relative reference
    IMAGE_REL_M32R_TOKEN      =    0x000E   // clr token
}

public struct IMAGE_RELOCATION
{
    public DWORD VirtualAddress;
//union        DWORD   RelocCount;             // Set to the real count when IMAGE_SCN_LNK_NRELOC_OVFL is set

  public DWORD  SymbolTableIndex;
  public RelType    Type;

    public static IMAGE_RELOCATION LoadFrom(BinaryReader r)
    {
        IMAGE_RELOCATION h = new IMAGE_RELOCATION();
        h.VirtualAddress = r.ReadUInt32();
        h.SymbolTableIndex = r.ReadUInt32();
        h.Type = (RelType)r.ReadUInt16();
        return h;
    }
}


[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
public struct SYSTEM_INFO {
    public InternalSYSTEM_INFO ProcessorInformation;
    public int dwPageSize;
    public int lpMinimumApplicationAddress;
    public int lpMaximumApplicationAddress;
    public int dwActiveProcessorMask;
    public int dwNumberOfProcessors;
    public int dwProcessorType;
    public int dwAllocationGranularity;
    public int dwProcessorLevel;
    public int dwProcessorRevision;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
public struct InternalSYSTEM_INFO {
    public Architecture wProcessorArchitecture;
    public short wReserved;
}

public enum Architecture : ushort {
    PROCESSOR_ARCHITECTURE_AMD64 = 9, //Extended 64-bit
    PROCESSOR_ARCHITECTURE_IA64 = 6, //Itanium 64-bit
    PROCESSOR_ARCHITECTURE_INTEL = 0, //32-bit
    PROCESSOR_ARCHITECTURE_UNKNOWN = 0xFFFF //Unknown
}

[Flags]
public enum FormatMessageFlags {
    FORMAT_MESSAGE_FROM_HMODULE = 0x00000800,
    FORMAT_MESSAGE_ARGUMENT_ARRAY = 0x00002000
}

[Flags]
public enum Rights : uint {
    GENERIC_READ = 0x80000000,
    GENERIC_WRITE = 0x40000000
}

[Flags]
public enum Share : uint {
    FILE_SHARE_READ = 1,
    FILE_SHARE_WRITE = 2,
    FILE_SHARE_DELETE = 4
}

[Flags]
public enum Disposition : uint {
    CREATE_NEW     = 1,
    CREATE_ALWAYS  = 2,
    OPEN_EXISTING  = 3,
    OPEN_ALWAYS    = 4,
    TRUNCATE_EXISTING = 5
}

public struct BY_HANDLE_FILE_INFORMATION {
    public DWORD dwFileAttributes;
    public FILETIME ftCreationTime;
    public FILETIME ftLastAccessTime;
    public FILETIME ftLastWriteTime;
    public DWORD dwVolumeSerialNumber;
    public DWORD nFileSizeHigh;
    public DWORD nFileSizeLow;
    public DWORD nNumberOfLinks;
    public DWORD nFileIndexHigh;
    public DWORD nFileIndexLow;
}

public partial class Api
{

#if UCFG_CF
    private const string kernel32 = "coredll.dll";
#else
        private const string kernel32 = "kernel32.dll";
#endif

    public const uint INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF;

    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr GetStdHandle(StdHandleNumber nStdHandle);

    [DllImport("kernel32", SetLastError = true)]
    public static extern FileType GetFileType(SafeFileHandle hFile);

    [DllImport("kernel32",SetLastError = true)]
    public static extern int TerminateProcess(HANDLE hProcess,int uExitCode);

    [DllImport("kernel32",SetLastError = true)]
    public static extern bool GenerateConsoleCtrlEvent(CtrlEvent dwCtrlEvent,int dwProcessGroupId);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
    public static extern int GetShortPathName([MarshalAs(UnmanagedType.LPTStr)] string path,
                                                [MarshalAs(UnmanagedType.LPTStr)] StringBuilder shortPath,
                                                int shortPathLength);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
    public static extern void GetSystemInfo([Out] out SYSTEM_INFO systemInfo);

    [DllImport(kernel32, SetLastError = true)]
    public static extern bool QueryPerformanceCounter(out long lpPerformanceCount);

    [DllImport(kernel32, SetLastError = true)]
    public static extern bool QueryPerformanceFrequency(out long lpFrequency);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr h);


    [DllImport("Kernel32.dll", EntryPoint = "GetFileAttributes", SetLastError = true)]
    public static extern uint GetFileAttributes(string lpFileName);

    [DllImport("Kernel32.dll", EntryPoint = "SetFileAttributes", SetLastError = true)]
    public static extern bool SetFileAttributes(string lpFileName, uint dwFileAttributes);

    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr GetModuleHandle(string moduleName);

    [DllImport("Kernel32.dll", SetLastError = true)]
    public static extern int FormatMessage(FormatMessageFlags flags, IntPtr source, int messageId, int languageId, StringBuilder buffer, int size, IntPtr arguments);


    [DllImport("Kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern SafeFileHandle CreateFile(string lpFileName, Rights dwDesiredAccess, Share dwShareMode, IntPtr lpSecurityAttributes, Disposition dwCreationDisposition, int dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("Kernel32", SetLastError = true)]
    public static extern bool GetFileInformationByHandle(SafeFileHandle h, out BY_HANDLE_FILE_INFORMATION info);


//  [DllImport("kernel32",SetLastError = true)]
//  public static extern int ReadFile(HANDLE hFile,IntPtr lpBuffer,int nNumberOfBytesToRead,ref int lpNumberOfBytesRead,ref OVERLAPPED lpOverlapped);
}


}
