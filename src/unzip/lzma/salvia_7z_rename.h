/* Salvia: renombrado de los simbolos publicos del LZMA SDK (copia en
 * src/unzip/lzma, solo descompresion de .7z).
 *
 * Varios cores (mGBA, FBNeo, Beetle, Genesis Plus GX, MAME 2003+...) traen su
 * propia copia del SDK y se enlazan en el mismo ejecutable que Salvia; sin
 * prefijo, CrcGenerateTable, LzmaDec_DecodeToDic, SzArEx_Open... estarian
 * definidos dos veces (LNK2005). Se incluye desde 7zTypes.h, que incluyen todos
 * los fuentes del SDK y sevenzip.cpp, asi que el prefijo se aplica igual en la
 * definicion y en las llamadas. */
#ifndef SALVIA_7Z_RENAME_H
#define SALVIA_7Z_RENAME_H

#define SzAlloc                        s7z_SzAlloc
#define SzFree                         s7z_SzFree
#define SzAllocTemp                    s7z_SzAllocTemp
#define SzFreeTemp                     s7z_SzFreeTemp
#define SzBitUi32s_Free                s7z_SzBitUi32s_Free
#define SzBitUi64s_Free                s7z_SzBitUi64s_Free
#define SzArEx_Init                    s7z_SzArEx_Init
#define SzArEx_Free                    s7z_SzArEx_Free
#define SzGetNextFolderItem            s7z_SzGetNextFolderItem
#define SzAr_GetFolderUnpackSize       s7z_SzAr_GetFolderUnpackSize
#define SzArEx_Open                    s7z_SzArEx_Open
#define SzArEx_Extract                 s7z_SzArEx_Extract
#define SzArEx_GetFileNameUtf16        s7z_SzArEx_GetFileNameUtf16
#define SzArEx_GetFullNameLen          s7z_SzArEx_GetFullNameLen
#define SzArEx_GetFullNameUtf16_Back   s7z_SzArEx_GetFullNameUtf16_Back
#define k7zSignature                   s7z_k7zSignature
#define Buf_Init                       s7z_Buf_Init
#define Buf_Create                     s7z_Buf_Create
#define Buf_Free                       s7z_Buf_Free
#define CrcUpdate                      s7z_CrcUpdate
#define CrcCalc                        s7z_CrcCalc
#define CrcUpdateT1                    s7z_CrcUpdateT1
#define CrcGenerateTable               s7z_CrcGenerateTable
#define CrcUpdateT4                    s7z_CrcUpdateT4
#define CrcUpdateT8                    s7z_CrcUpdateT8
#define CrcUpdateT1_BeT4               s7z_CrcUpdateT1_BeT4
#define CrcUpdateT1_BeT8               s7z_CrcUpdateT1_BeT8
#define g_CrcTable                     s7z_g_CrcTable
#define g_CrcUpdateT4                  s7z_g_CrcUpdateT4
#define g_CrcUpdateT8                  s7z_g_CrcUpdateT8
#define g_CrcUpdate                    s7z_g_CrcUpdate
#define SzAr_DecodeFolder              s7z_SzAr_DecodeFolder
#define File_Construct                 s7z_File_Construct
#define InFile_Open                    s7z_InFile_Open
#define OutFile_Open                   s7z_OutFile_Open
#define InFile_OpenW                   s7z_InFile_OpenW
#define OutFile_OpenW                  s7z_OutFile_OpenW
#define File_Close                     s7z_File_Close
#define File_Read                      s7z_File_Read
#define File_Write                     s7z_File_Write
#define File_Seek                      s7z_File_Seek
#define File_GetLength                 s7z_File_GetLength
#define FileSeqInStream_CreateVTable   s7z_FileSeqInStream_CreateVTable
#define FileInStream_CreateVTable      s7z_FileInStream_CreateVTable
#define FileOutStream_CreateVTable     s7z_FileOutStream_CreateVTable
#define SeqInStream_Read2              s7z_SeqInStream_Read2
#define SeqInStream_Read               s7z_SeqInStream_Read
#define SeqInStream_ReadByte           s7z_SeqInStream_ReadByte
#define LookInStream_SeekTo            s7z_LookInStream_SeekTo
#define LookInStream_LookRead          s7z_LookInStream_LookRead
#define LookInStream_Read2             s7z_LookInStream_Read2
#define LookInStream_Read              s7z_LookInStream_Read
#define LookToRead2_CreateVTable       s7z_LookToRead2_CreateVTable
#define SecToLook_CreateVTable         s7z_SecToLook_CreateVTable
#define SecToRead_CreateVTable         s7z_SecToRead_CreateVTable
#define Bcj2Dec_Init                   s7z_Bcj2Dec_Init
#define Bcj2Dec_Decode                 s7z_Bcj2Dec_Decode
#define ARM_Convert                    s7z_ARM_Convert
#define ARMT_Convert                   s7z_ARMT_Convert
#define PPC_Convert                    s7z_PPC_Convert
#define SPARC_Convert                  s7z_SPARC_Convert
#define x86_Convert                    s7z_x86_Convert
#define IA64_Convert                   s7z_IA64_Convert
#define MyCPUID                        s7z_MyCPUID
#define x86cpuid_CheckAndRead          s7z_x86cpuid_CheckAndRead
#define x86cpuid_GetFirm               s7z_x86cpuid_GetFirm
#define CPU_Is_InOrder                 s7z_CPU_Is_InOrder
#define CPU_Is_Aes_Supported           s7z_CPU_Is_Aes_Supported
#define CPU_IsSupported_PageGB         s7z_CPU_IsSupported_PageGB
#define Delta_Init                     s7z_Delta_Init
#define Delta_Encode                   s7z_Delta_Encode
#define Delta_Decode                   s7z_Delta_Decode
#define Lzma2Dec_AllocateProbs         s7z_Lzma2Dec_AllocateProbs
#define Lzma2Dec_Allocate              s7z_Lzma2Dec_Allocate
#define Lzma2Dec_Init                  s7z_Lzma2Dec_Init
#define Lzma2Dec_DecodeToDic           s7z_Lzma2Dec_DecodeToDic
#define Lzma2Dec_Parse                 s7z_Lzma2Dec_Parse
#define Lzma2Dec_DecodeToBuf           s7z_Lzma2Dec_DecodeToBuf
#define Lzma2Decode                    s7z_Lzma2Decode
#define LzmaDec_DecodeReal_3           s7z_LzmaDec_DecodeReal_3
#define LzmaDec_InitDicAndState        s7z_LzmaDec_InitDicAndState
#define LzmaDec_Init                   s7z_LzmaDec_Init
#define LzmaDec_DecodeToDic            s7z_LzmaDec_DecodeToDic
#define LzmaDec_DecodeToBuf            s7z_LzmaDec_DecodeToBuf
#define LzmaDec_FreeProbs              s7z_LzmaDec_FreeProbs
#define LzmaDec_Free                   s7z_LzmaDec_Free
#define LzmaProps_Decode               s7z_LzmaProps_Decode
#define LzmaDec_AllocateProbs          s7z_LzmaDec_AllocateProbs
#define LzmaDec_Allocate               s7z_LzmaDec_Allocate
#define LzmaDecode                     s7z_LzmaDecode

#endif /* SALVIA_7Z_RENAME_H */
