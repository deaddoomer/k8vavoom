//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2020 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
//**
//**  This is a 16 bit, non-reflected CRC using the polynomial 0x1021 and
//**  the initial and final xor values shown below...  in other words, the
//**  CCITT standard CRC used by XMODEM
//**
//**************************************************************************
#include "../core.h"


const vuint32 TCRC16::crc16Table[256] = {
  0x0000U, 0x1021U, 0x2042U, 0x3063U, 0x4084U, 0x50a5U, 0x60c6U, 0x70e7U,
  0x8108U, 0x9129U, 0xa14aU, 0xb16bU, 0xc18cU, 0xd1adU, 0xe1ceU, 0xf1efU,
  0x1231U, 0x0210U, 0x3273U, 0x2252U, 0x52b5U, 0x4294U, 0x72f7U, 0x62d6U,
  0x9339U, 0x8318U, 0xb37bU, 0xa35aU, 0xd3bdU, 0xc39cU, 0xf3ffU, 0xe3deU,
  0x2462U, 0x3443U, 0x0420U, 0x1401U, 0x64e6U, 0x74c7U, 0x44a4U, 0x5485U,
  0xa56aU, 0xb54bU, 0x8528U, 0x9509U, 0xe5eeU, 0xf5cfU, 0xc5acU, 0xd58dU,
  0x3653U, 0x2672U, 0x1611U, 0x0630U, 0x76d7U, 0x66f6U, 0x5695U, 0x46b4U,
  0xb75bU, 0xa77aU, 0x9719U, 0x8738U, 0xf7dfU, 0xe7feU, 0xd79dU, 0xc7bcU,
  0x48c4U, 0x58e5U, 0x6886U, 0x78a7U, 0x0840U, 0x1861U, 0x2802U, 0x3823U,
  0xc9ccU, 0xd9edU, 0xe98eU, 0xf9afU, 0x8948U, 0x9969U, 0xa90aU, 0xb92bU,
  0x5af5U, 0x4ad4U, 0x7ab7U, 0x6a96U, 0x1a71U, 0x0a50U, 0x3a33U, 0x2a12U,
  0xdbfdU, 0xcbdcU, 0xfbbfU, 0xeb9eU, 0x9b79U, 0x8b58U, 0xbb3bU, 0xab1aU,
  0x6ca6U, 0x7c87U, 0x4ce4U, 0x5cc5U, 0x2c22U, 0x3c03U, 0x0c60U, 0x1c41U,
  0xedaeU, 0xfd8fU, 0xcdecU, 0xddcdU, 0xad2aU, 0xbd0bU, 0x8d68U, 0x9d49U,
  0x7e97U, 0x6eb6U, 0x5ed5U, 0x4ef4U, 0x3e13U, 0x2e32U, 0x1e51U, 0x0e70U,
  0xff9fU, 0xefbeU, 0xdfddU, 0xcffcU, 0xbf1bU, 0xaf3aU, 0x9f59U, 0x8f78U,
  0x9188U, 0x81a9U, 0xb1caU, 0xa1ebU, 0xd10cU, 0xc12dU, 0xf14eU, 0xe16fU,
  0x1080U, 0x00a1U, 0x30c2U, 0x20e3U, 0x5004U, 0x4025U, 0x7046U, 0x6067U,
  0x83b9U, 0x9398U, 0xa3fbU, 0xb3daU, 0xc33dU, 0xd31cU, 0xe37fU, 0xf35eU,
  0x02b1U, 0x1290U, 0x22f3U, 0x32d2U, 0x4235U, 0x5214U, 0x6277U, 0x7256U,
  0xb5eaU, 0xa5cbU, 0x95a8U, 0x8589U, 0xf56eU, 0xe54fU, 0xd52cU, 0xc50dU,
  0x34e2U, 0x24c3U, 0x14a0U, 0x0481U, 0x7466U, 0x6447U, 0x5424U, 0x4405U,
  0xa7dbU, 0xb7faU, 0x8799U, 0x97b8U, 0xe75fU, 0xf77eU, 0xc71dU, 0xd73cU,
  0x26d3U, 0x36f2U, 0x0691U, 0x16b0U, 0x6657U, 0x7676U, 0x4615U, 0x5634U,
  0xd94cU, 0xc96dU, 0xf90eU, 0xe92fU, 0x99c8U, 0x89e9U, 0xb98aU, 0xa9abU,
  0x5844U, 0x4865U, 0x7806U, 0x6827U, 0x18c0U, 0x08e1U, 0x3882U, 0x28a3U,
  0xcb7dU, 0xdb5cU, 0xeb3fU, 0xfb1eU, 0x8bf9U, 0x9bd8U, 0xabbbU, 0xbb9aU,
  0x4a75U, 0x5a54U, 0x6a37U, 0x7a16U, 0x0af1U, 0x1ad0U, 0x2ab3U, 0x3a92U,
  0xfd2eU, 0xed0fU, 0xdd6cU, 0xcd4dU, 0xbdaaU, 0xad8bU, 0x9de8U, 0x8dc9U,
  0x7c26U, 0x6c07U, 0x5c64U, 0x4c45U, 0x3ca2U, 0x2c83U, 0x1ce0U, 0x0cc1U,
  0xef1fU, 0xff3eU, 0xcf5dU, 0xdf7cU, 0xaf9bU, 0xbfbaU, 0x8fd9U, 0x9ff8U,
  0x6e17U, 0x7e36U, 0x4e55U, 0x5e74U, 0x2e93U, 0x3eb2U, 0x0ed1U, 0x1ef0U,
};


const vuint32 TCRC32::crc32Table[16] = {
  0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
  0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
  0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
  0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
};


/* from RFC 3309
#define CRC32C_POLY 0x1EDC6F41
#define CRC32C(c,d) (c=(c>>8)^crc_c[(c^(d))&0xFF])

static const vuint32 crc_c[256] = {
  0x00000000U, 0xF26B8303U, 0xE13B70F7U, 0x1350F3F4U,
  0xC79A971FU, 0x35F1141CU, 0x26A1E7E8U, 0xD4CA64EBU,
  0x8AD958CFU, 0x78B2DBCCU, 0x6BE22838U, 0x9989AB3BU,
  0x4D43CFD0U, 0xBF284CD3U, 0xAC78BF27U, 0x5E133C24U,
  0x105EC76FU, 0xE235446CU, 0xF165B798U, 0x030E349BU,
  0xD7C45070U, 0x25AFD373U, 0x36FF2087U, 0xC494A384U,
  0x9A879FA0U, 0x68EC1CA3U, 0x7BBCEF57U, 0x89D76C54U,
  0x5D1D08BFU, 0xAF768BBCU, 0xBC267848U, 0x4E4DFB4BU,
  0x20BD8EDEU, 0xD2D60DDDU, 0xC186FE29U, 0x33ED7D2AU,
  0xE72719C1U, 0x154C9AC2U, 0x061C6936U, 0xF477EA35U,
  0xAA64D611U, 0x580F5512U, 0x4B5FA6E6U, 0xB93425E5U,
  0x6DFE410EU, 0x9F95C20DU, 0x8CC531F9U, 0x7EAEB2FAU,
  0x30E349B1U, 0xC288CAB2U, 0xD1D83946U, 0x23B3BA45U,
  0xF779DEAEU, 0x05125DADU, 0x1642AE59U, 0xE4292D5AU,
  0xBA3A117EU, 0x4851927DU, 0x5B016189U, 0xA96AE28AU,
  0x7DA08661U, 0x8FCB0562U, 0x9C9BF696U, 0x6EF07595U,
  0x417B1DBCU, 0xB3109EBFU, 0xA0406D4BU, 0x522BEE48U,
  0x86E18AA3U, 0x748A09A0U, 0x67DAFA54U, 0x95B17957U,
  0xCBA24573U, 0x39C9C670U, 0x2A993584U, 0xD8F2B687U,
  0x0C38D26CU, 0xFE53516FU, 0xED03A29BU, 0x1F682198U,
  0x5125DAD3U, 0xA34E59D0U, 0xB01EAA24U, 0x42752927U,
  0x96BF4DCCU, 0x64D4CECFU, 0x77843D3BU, 0x85EFBE38U,
  0xDBFC821CU, 0x2997011FU, 0x3AC7F2EBU, 0xC8AC71E8U,
  0x1C661503U, 0xEE0D9600U, 0xFD5D65F4U, 0x0F36E6F7U,
  0x61C69362U, 0x93AD1061U, 0x80FDE395U, 0x72966096U,
  0xA65C047DU, 0x5437877EU, 0x4767748AU, 0xB50CF789U,
  0xEB1FCBADU, 0x197448AEU, 0x0A24BB5AU, 0xF84F3859U,
  0x2C855CB2U, 0xDEEEDFB1U, 0xCDBE2C45U, 0x3FD5AF46U,
  0x7198540DU, 0x83F3D70EU, 0x90A324FAU, 0x62C8A7F9U,
  0xB602C312U, 0x44694011U, 0x5739B3E5U, 0xA55230E6U,
  0xFB410CC2U, 0x092A8FC1U, 0x1A7A7C35U, 0xE811FF36U,
  0x3CDB9BDDU, 0xCEB018DEU, 0xDDE0EB2AU, 0x2F8B6829U,
  0x82F63B78U, 0x709DB87BU, 0x63CD4B8FU, 0x91A6C88CU,
  0x456CAC67U, 0xB7072F64U, 0xA457DC90U, 0x563C5F93U,
  0x082F63B7U, 0xFA44E0B4U, 0xE9141340U, 0x1B7F9043U,
  0xCFB5F4A8U, 0x3DDE77ABU, 0x2E8E845FU, 0xDCE5075CU,
  0x92A8FC17U, 0x60C37F14U, 0x73938CE0U, 0x81F80FE3U,
  0x55326B08U, 0xA759E80BU, 0xB4091BFFU, 0x466298FCU,
  0x1871A4D8U, 0xEA1A27DBU, 0xF94AD42FU, 0x0B21572CU,
  0xDFEB33C7U, 0x2D80B0C4U, 0x3ED04330U, 0xCCBBC033U,
  0xA24BB5A6U, 0x502036A5U, 0x4370C551U, 0xB11B4652U,
  0x65D122B9U, 0x97BAA1BAU, 0x84EA524EU, 0x7681D14DU,
  0x2892ED69U, 0xDAF96E6AU, 0xC9A99D9EU, 0x3BC21E9DU,
  0xEF087A76U, 0x1D63F975U, 0x0E330A81U, 0xFC588982U,
  0xB21572C9U, 0x407EF1CAU, 0x532E023EU, 0xA145813DU,
  0x758FE5D6U, 0x87E466D5U, 0x94B49521U, 0x66DF1622U,
  0x38CC2A06U, 0xCAA7A905U, 0xD9F75AF1U, 0x2B9CD9F2U,
  0xFF56BD19U, 0x0D3D3E1AU, 0x1E6DCDEEU, 0xEC064EEDU,
  0xC38D26C4U, 0x31E6A5C7U, 0x22B65633U, 0xD0DDD530U,
  0x0417B1DBU, 0xF67C32D8U, 0xE52CC12CU, 0x1747422FU,
  0x49547E0BU, 0xBB3FFD08U, 0xA86F0EFCU, 0x5A048DFFU,
  0x8ECEE914U, 0x7CA56A17U, 0x6FF599E3U, 0x9D9E1AE0U,
  0xD3D3E1ABU, 0x21B862A8U, 0x32E8915CU, 0xC083125FU,
  0x144976B4U, 0xE622F5B7U, 0xF5720643U, 0x07198540U,
  0x590AB964U, 0xAB613A67U, 0xB831C993U, 0x4A5A4A90U,
  0x9E902E7BU, 0x6CFBAD78U, 0x7FAB5E8CU, 0x8DC0DD8FU,
  0xE330A81AU, 0x115B2B19U, 0x020BD8EDU, 0xF0605BEEU,
  0x24AA3F05U, 0xD6C1BC06U, 0xC5914FF2U, 0x37FACCF1U,
  0x69E9F0D5U, 0x9B8273D6U, 0x88D28022U, 0x7AB90321U,
  0xAE7367CAU, 0x5C18E4C9U, 0x4F48173DU, 0xBD23943EU,
  0xF36E6F75U, 0x0105EC76U, 0x12551F82U, 0xE03E9C81U,
  0x34F4F86AU, 0xC69F7B69U, 0xD5CF889DU, 0x27A40B9EU,
  0x79B737BAU, 0x8BDCB4B9U, 0x988C474DU, 0x6AE7C44EU,
  0xBE2DA0A5U, 0x4C4623A6U, 0x5F16D052U, 0xAD7D5351U,
};


vuint32 calc_crc32c (vuint32 crc32, const void *buf, size_t length) {
  crc32 = ~crc32;
  const vuint8 *buffer = (const vuint8 *)buf;
  for (size_t i = 0; i < length; ++i) {
    CRC32C(crc32, buffer[i]);
  }
  crc32 = ~crc32;
  return crc32;
}
*/


// based on intel's slice-by-8 idea
static const vuint32 crcctbl32[256] = {
 0x00000000U, 0xF26B8303U, 0xE13B70F7U, 0x1350F3F4U, 0xC79A971FU, 0x35F1141CU, 0x26A1E7E8U, 0xD4CA64EBU,
 0x8AD958CFU, 0x78B2DBCCU, 0x6BE22838U, 0x9989AB3BU, 0x4D43CFD0U, 0xBF284CD3U, 0xAC78BF27U, 0x5E133C24U,
 0x105EC76FU, 0xE235446CU, 0xF165B798U, 0x030E349BU, 0xD7C45070U, 0x25AFD373U, 0x36FF2087U, 0xC494A384U,
 0x9A879FA0U, 0x68EC1CA3U, 0x7BBCEF57U, 0x89D76C54U, 0x5D1D08BFU, 0xAF768BBCU, 0xBC267848U, 0x4E4DFB4BU,
 0x20BD8EDEU, 0xD2D60DDDU, 0xC186FE29U, 0x33ED7D2AU, 0xE72719C1U, 0x154C9AC2U, 0x061C6936U, 0xF477EA35U,
 0xAA64D611U, 0x580F5512U, 0x4B5FA6E6U, 0xB93425E5U, 0x6DFE410EU, 0x9F95C20DU, 0x8CC531F9U, 0x7EAEB2FAU,
 0x30E349B1U, 0xC288CAB2U, 0xD1D83946U, 0x23B3BA45U, 0xF779DEAEU, 0x05125DADU, 0x1642AE59U, 0xE4292D5AU,
 0xBA3A117EU, 0x4851927DU, 0x5B016189U, 0xA96AE28AU, 0x7DA08661U, 0x8FCB0562U, 0x9C9BF696U, 0x6EF07595U,
 0x417B1DBCU, 0xB3109EBFU, 0xA0406D4BU, 0x522BEE48U, 0x86E18AA3U, 0x748A09A0U, 0x67DAFA54U, 0x95B17957U,
 0xCBA24573U, 0x39C9C670U, 0x2A993584U, 0xD8F2B687U, 0x0C38D26CU, 0xFE53516FU, 0xED03A29BU, 0x1F682198U,
 0x5125DAD3U, 0xA34E59D0U, 0xB01EAA24U, 0x42752927U, 0x96BF4DCCU, 0x64D4CECFU, 0x77843D3BU, 0x85EFBE38U,
 0xDBFC821CU, 0x2997011FU, 0x3AC7F2EBU, 0xC8AC71E8U, 0x1C661503U, 0xEE0D9600U, 0xFD5D65F4U, 0x0F36E6F7U,
 0x61C69362U, 0x93AD1061U, 0x80FDE395U, 0x72966096U, 0xA65C047DU, 0x5437877EU, 0x4767748AU, 0xB50CF789U,
 0xEB1FCBADU, 0x197448AEU, 0x0A24BB5AU, 0xF84F3859U, 0x2C855CB2U, 0xDEEEDFB1U, 0xCDBE2C45U, 0x3FD5AF46U,
 0x7198540DU, 0x83F3D70EU, 0x90A324FAU, 0x62C8A7F9U, 0xB602C312U, 0x44694011U, 0x5739B3E5U, 0xA55230E6U,
 0xFB410CC2U, 0x092A8FC1U, 0x1A7A7C35U, 0xE811FF36U, 0x3CDB9BDDU, 0xCEB018DEU, 0xDDE0EB2AU, 0x2F8B6829U,
 0x82F63B78U, 0x709DB87BU, 0x63CD4B8FU, 0x91A6C88CU, 0x456CAC67U, 0xB7072F64U, 0xA457DC90U, 0x563C5F93U,
 0x082F63B7U, 0xFA44E0B4U, 0xE9141340U, 0x1B7F9043U, 0xCFB5F4A8U, 0x3DDE77ABU, 0x2E8E845FU, 0xDCE5075CU,
 0x92A8FC17U, 0x60C37F14U, 0x73938CE0U, 0x81F80FE3U, 0x55326B08U, 0xA759E80BU, 0xB4091BFFU, 0x466298FCU,
 0x1871A4D8U, 0xEA1A27DBU, 0xF94AD42FU, 0x0B21572CU, 0xDFEB33C7U, 0x2D80B0C4U, 0x3ED04330U, 0xCCBBC033U,
 0xA24BB5A6U, 0x502036A5U, 0x4370C551U, 0xB11B4652U, 0x65D122B9U, 0x97BAA1BAU, 0x84EA524EU, 0x7681D14DU,
 0x2892ED69U, 0xDAF96E6AU, 0xC9A99D9EU, 0x3BC21E9DU, 0xEF087A76U, 0x1D63F975U, 0x0E330A81U, 0xFC588982U,
 0xB21572C9U, 0x407EF1CAU, 0x532E023EU, 0xA145813DU, 0x758FE5D6U, 0x87E466D5U, 0x94B49521U, 0x66DF1622U,
 0x38CC2A06U, 0xCAA7A905U, 0xD9F75AF1U, 0x2B9CD9F2U, 0xFF56BD19U, 0x0D3D3E1AU, 0x1E6DCDEEU, 0xEC064EEDU,
 0xC38D26C4U, 0x31E6A5C7U, 0x22B65633U, 0xD0DDD530U, 0x0417B1DBU, 0xF67C32D8U, 0xE52CC12CU, 0x1747422FU,
 0x49547E0BU, 0xBB3FFD08U, 0xA86F0EFCU, 0x5A048DFFU, 0x8ECEE914U, 0x7CA56A17U, 0x6FF599E3U, 0x9D9E1AE0U,
 0xD3D3E1ABU, 0x21B862A8U, 0x32E8915CU, 0xC083125FU, 0x144976B4U, 0xE622F5B7U, 0xF5720643U, 0x07198540U,
 0x590AB964U, 0xAB613A67U, 0xB831C993U, 0x4A5A4A90U, 0x9E902E7BU, 0x6CFBAD78U, 0x7FAB5E8CU, 0x8DC0DD8FU,
 0xE330A81AU, 0x115B2B19U, 0x020BD8EDU, 0xF0605BEEU, 0x24AA3F05U, 0xD6C1BC06U, 0xC5914FF2U, 0x37FACCF1U,
 0x69E9F0D5U, 0x9B8273D6U, 0x88D28022U, 0x7AB90321U, 0xAE7367CAU, 0x5C18E4C9U, 0x4F48173DU, 0xBD23943EU,
 0xF36E6F75U, 0x0105EC76U, 0x12551F82U, 0xE03E9C81U, 0x34F4F86AU, 0xC69F7B69U, 0xD5CF889DU, 0x27A40B9EU,
 0x79B737BAU, 0x8BDCB4B9U, 0x988C474DU, 0x6AE7C44EU, 0xBE2DA0A5U, 0x4C4623A6U, 0x5F16D052U, 0xAD7D5351U,
};

static const vuint32 crcctbl40[256] = {
 0x00000000U, 0x13A29877U, 0x274530EEU, 0x34E7A899U, 0x4E8A61DCU, 0x5D28F9ABU, 0x69CF5132U, 0x7A6DC945U,
 0x9D14C3B8U, 0x8EB65BCFU, 0xBA51F356U, 0xA9F36B21U, 0xD39EA264U, 0xC03C3A13U, 0xF4DB928AU, 0xE7790AFDU,
 0x3FC5F181U, 0x2C6769F6U, 0x1880C16FU, 0x0B225918U, 0x714F905DU, 0x62ED082AU, 0x560AA0B3U, 0x45A838C4U,
 0xA2D13239U, 0xB173AA4EU, 0x859402D7U, 0x96369AA0U, 0xEC5B53E5U, 0xFFF9CB92U, 0xCB1E630BU, 0xD8BCFB7CU,
 0x7F8BE302U, 0x6C297B75U, 0x58CED3ECU, 0x4B6C4B9BU, 0x310182DEU, 0x22A31AA9U, 0x1644B230U, 0x05E62A47U,
 0xE29F20BAU, 0xF13DB8CDU, 0xC5DA1054U, 0xD6788823U, 0xAC154166U, 0xBFB7D911U, 0x8B507188U, 0x98F2E9FFU,
 0x404E1283U, 0x53EC8AF4U, 0x670B226DU, 0x74A9BA1AU, 0x0EC4735FU, 0x1D66EB28U, 0x298143B1U, 0x3A23DBC6U,
 0xDD5AD13BU, 0xCEF8494CU, 0xFA1FE1D5U, 0xE9BD79A2U, 0x93D0B0E7U, 0x80722890U, 0xB4958009U, 0xA737187EU,
 0xFF17C604U, 0xECB55E73U, 0xD852F6EAU, 0xCBF06E9DU, 0xB19DA7D8U, 0xA23F3FAFU, 0x96D89736U, 0x857A0F41U,
 0x620305BCU, 0x71A19DCBU, 0x45463552U, 0x56E4AD25U, 0x2C896460U, 0x3F2BFC17U, 0x0BCC548EU, 0x186ECCF9U,
 0xC0D23785U, 0xD370AFF2U, 0xE797076BU, 0xF4359F1CU, 0x8E585659U, 0x9DFACE2EU, 0xA91D66B7U, 0xBABFFEC0U,
 0x5DC6F43DU, 0x4E646C4AU, 0x7A83C4D3U, 0x69215CA4U, 0x134C95E1U, 0x00EE0D96U, 0x3409A50FU, 0x27AB3D78U,
 0x809C2506U, 0x933EBD71U, 0xA7D915E8U, 0xB47B8D9FU, 0xCE1644DAU, 0xDDB4DCADU, 0xE9537434U, 0xFAF1EC43U,
 0x1D88E6BEU, 0x0E2A7EC9U, 0x3ACDD650U, 0x296F4E27U, 0x53028762U, 0x40A01F15U, 0x7447B78CU, 0x67E52FFBU,
 0xBF59D487U, 0xACFB4CF0U, 0x981CE469U, 0x8BBE7C1EU, 0xF1D3B55BU, 0xE2712D2CU, 0xD69685B5U, 0xC5341DC2U,
 0x224D173FU, 0x31EF8F48U, 0x050827D1U, 0x16AABFA6U, 0x6CC776E3U, 0x7F65EE94U, 0x4B82460DU, 0x5820DE7AU,
 0xFBC3FAF9U, 0xE861628EU, 0xDC86CA17U, 0xCF245260U, 0xB5499B25U, 0xA6EB0352U, 0x920CABCBU, 0x81AE33BCU,
 0x66D73941U, 0x7575A136U, 0x419209AFU, 0x523091D8U, 0x285D589DU, 0x3BFFC0EAU, 0x0F186873U, 0x1CBAF004U,
 0xC4060B78U, 0xD7A4930FU, 0xE3433B96U, 0xF0E1A3E1U, 0x8A8C6AA4U, 0x992EF2D3U, 0xADC95A4AU, 0xBE6BC23DU,
 0x5912C8C0U, 0x4AB050B7U, 0x7E57F82EU, 0x6DF56059U, 0x1798A91CU, 0x043A316BU, 0x30DD99F2U, 0x237F0185U,
 0x844819FBU, 0x97EA818CU, 0xA30D2915U, 0xB0AFB162U, 0xCAC27827U, 0xD960E050U, 0xED8748C9U, 0xFE25D0BEU,
 0x195CDA43U, 0x0AFE4234U, 0x3E19EAADU, 0x2DBB72DAU, 0x57D6BB9FU, 0x447423E8U, 0x70938B71U, 0x63311306U,
 0xBB8DE87AU, 0xA82F700DU, 0x9CC8D894U, 0x8F6A40E3U, 0xF50789A6U, 0xE6A511D1U, 0xD242B948U, 0xC1E0213FU,
 0x26992BC2U, 0x353BB3B5U, 0x01DC1B2CU, 0x127E835BU, 0x68134A1EU, 0x7BB1D269U, 0x4F567AF0U, 0x5CF4E287U,
 0x04D43CFDU, 0x1776A48AU, 0x23910C13U, 0x30339464U, 0x4A5E5D21U, 0x59FCC556U, 0x6D1B6DCFU, 0x7EB9F5B8U,
 0x99C0FF45U, 0x8A626732U, 0xBE85CFABU, 0xAD2757DCU, 0xD74A9E99U, 0xC4E806EEU, 0xF00FAE77U, 0xE3AD3600U,
 0x3B11CD7CU, 0x28B3550BU, 0x1C54FD92U, 0x0FF665E5U, 0x759BACA0U, 0x663934D7U, 0x52DE9C4EU, 0x417C0439U,
 0xA6050EC4U, 0xB5A796B3U, 0x81403E2AU, 0x92E2A65DU, 0xE88F6F18U, 0xFB2DF76FU, 0xCFCA5FF6U, 0xDC68C781U,
 0x7B5FDFFFU, 0x68FD4788U, 0x5C1AEF11U, 0x4FB87766U, 0x35D5BE23U, 0x26772654U, 0x12908ECDU, 0x013216BAU,
 0xE64B1C47U, 0xF5E98430U, 0xC10E2CA9U, 0xD2ACB4DEU, 0xA8C17D9BU, 0xBB63E5ECU, 0x8F844D75U, 0x9C26D502U,
 0x449A2E7EU, 0x5738B609U, 0x63DF1E90U, 0x707D86E7U, 0x0A104FA2U, 0x19B2D7D5U, 0x2D557F4CU, 0x3EF7E73BU,
 0xD98EEDC6U, 0xCA2C75B1U, 0xFECBDD28U, 0xED69455FU, 0x97048C1AU, 0x84A6146DU, 0xB041BCF4U, 0xA3E32483U,
};

static const vuint32 crcctbl48[256] = {
 0x00000000U, 0xA541927EU, 0x4F6F520DU, 0xEA2EC073U, 0x9EDEA41AU, 0x3B9F3664U, 0xD1B1F617U, 0x74F06469U,
 0x38513EC5U, 0x9D10ACBBU, 0x773E6CC8U, 0xD27FFEB6U, 0xA68F9ADFU, 0x03CE08A1U, 0xE9E0C8D2U, 0x4CA15AACU,
 0x70A27D8AU, 0xD5E3EFF4U, 0x3FCD2F87U, 0x9A8CBDF9U, 0xEE7CD990U, 0x4B3D4BEEU, 0xA1138B9DU, 0x045219E3U,
 0x48F3434FU, 0xEDB2D131U, 0x079C1142U, 0xA2DD833CU, 0xD62DE755U, 0x736C752BU, 0x9942B558U, 0x3C032726U,
 0xE144FB14U, 0x4405696AU, 0xAE2BA919U, 0x0B6A3B67U, 0x7F9A5F0EU, 0xDADBCD70U, 0x30F50D03U, 0x95B49F7DU,
 0xD915C5D1U, 0x7C5457AFU, 0x967A97DCU, 0x333B05A2U, 0x47CB61CBU, 0xE28AF3B5U, 0x08A433C6U, 0xADE5A1B8U,
 0x91E6869EU, 0x34A714E0U, 0xDE89D493U, 0x7BC846EDU, 0x0F382284U, 0xAA79B0FAU, 0x40577089U, 0xE516E2F7U,
 0xA9B7B85BU, 0x0CF62A25U, 0xE6D8EA56U, 0x43997828U, 0x37691C41U, 0x92288E3FU, 0x78064E4CU, 0xDD47DC32U,
 0xC76580D9U, 0x622412A7U, 0x880AD2D4U, 0x2D4B40AAU, 0x59BB24C3U, 0xFCFAB6BDU, 0x16D476CEU, 0xB395E4B0U,
 0xFF34BE1CU, 0x5A752C62U, 0xB05BEC11U, 0x151A7E6FU, 0x61EA1A06U, 0xC4AB8878U, 0x2E85480BU, 0x8BC4DA75U,
 0xB7C7FD53U, 0x12866F2DU, 0xF8A8AF5EU, 0x5DE93D20U, 0x29195949U, 0x8C58CB37U, 0x66760B44U, 0xC337993AU,
 0x8F96C396U, 0x2AD751E8U, 0xC0F9919BU, 0x65B803E5U, 0x1148678CU, 0xB409F5F2U, 0x5E273581U, 0xFB66A7FFU,
 0x26217BCDU, 0x8360E9B3U, 0x694E29C0U, 0xCC0FBBBEU, 0xB8FFDFD7U, 0x1DBE4DA9U, 0xF7908DDAU, 0x52D11FA4U,
 0x1E704508U, 0xBB31D776U, 0x511F1705U, 0xF45E857BU, 0x80AEE112U, 0x25EF736CU, 0xCFC1B31FU, 0x6A802161U,
 0x56830647U, 0xF3C29439U, 0x19EC544AU, 0xBCADC634U, 0xC85DA25DU, 0x6D1C3023U, 0x8732F050U, 0x2273622EU,
 0x6ED23882U, 0xCB93AAFCU, 0x21BD6A8FU, 0x84FCF8F1U, 0xF00C9C98U, 0x554D0EE6U, 0xBF63CE95U, 0x1A225CEBU,
 0x8B277743U, 0x2E66E53DU, 0xC448254EU, 0x6109B730U, 0x15F9D359U, 0xB0B84127U, 0x5A968154U, 0xFFD7132AU,
 0xB3764986U, 0x1637DBF8U, 0xFC191B8BU, 0x595889F5U, 0x2DA8ED9CU, 0x88E97FE2U, 0x62C7BF91U, 0xC7862DEFU,
 0xFB850AC9U, 0x5EC498B7U, 0xB4EA58C4U, 0x11ABCABAU, 0x655BAED3U, 0xC01A3CADU, 0x2A34FCDEU, 0x8F756EA0U,
 0xC3D4340CU, 0x6695A672U, 0x8CBB6601U, 0x29FAF47FU, 0x5D0A9016U, 0xF84B0268U, 0x1265C21BU, 0xB7245065U,
 0x6A638C57U, 0xCF221E29U, 0x250CDE5AU, 0x804D4C24U, 0xF4BD284DU, 0x51FCBA33U, 0xBBD27A40U, 0x1E93E83EU,
 0x5232B292U, 0xF77320ECU, 0x1D5DE09FU, 0xB81C72E1U, 0xCCEC1688U, 0x69AD84F6U, 0x83834485U, 0x26C2D6FBU,
 0x1AC1F1DDU, 0xBF8063A3U, 0x55AEA3D0U, 0xF0EF31AEU, 0x841F55C7U, 0x215EC7B9U, 0xCB7007CAU, 0x6E3195B4U,
 0x2290CF18U, 0x87D15D66U, 0x6DFF9D15U, 0xC8BE0F6BU, 0xBC4E6B02U, 0x190FF97CU, 0xF321390FU, 0x5660AB71U,
 0x4C42F79AU, 0xE90365E4U, 0x032DA597U, 0xA66C37E9U, 0xD29C5380U, 0x77DDC1FEU, 0x9DF3018DU, 0x38B293F3U,
 0x7413C95FU, 0xD1525B21U, 0x3B7C9B52U, 0x9E3D092CU, 0xEACD6D45U, 0x4F8CFF3BU, 0xA5A23F48U, 0x00E3AD36U,
 0x3CE08A10U, 0x99A1186EU, 0x738FD81DU, 0xD6CE4A63U, 0xA23E2E0AU, 0x077FBC74U, 0xED517C07U, 0x4810EE79U,
 0x04B1B4D5U, 0xA1F026ABU, 0x4BDEE6D8U, 0xEE9F74A6U, 0x9A6F10CFU, 0x3F2E82B1U, 0xD50042C2U, 0x7041D0BCU,
 0xAD060C8EU, 0x08479EF0U, 0xE2695E83U, 0x4728CCFDU, 0x33D8A894U, 0x96993AEAU, 0x7CB7FA99U, 0xD9F668E7U,
 0x9557324BU, 0x3016A035U, 0xDA386046U, 0x7F79F238U, 0x0B899651U, 0xAEC8042FU, 0x44E6C45CU, 0xE1A75622U,
 0xDDA47104U, 0x78E5E37AU, 0x92CB2309U, 0x378AB177U, 0x437AD51EU, 0xE63B4760U, 0x0C158713U, 0xA954156DU,
 0xE5F54FC1U, 0x40B4DDBFU, 0xAA9A1DCCU, 0x0FDB8FB2U, 0x7B2BEBDBU, 0xDE6A79A5U, 0x3444B9D6U, 0x91052BA8U,
};

static const vuint32 crcctbl56[256] = {
 0x00000000U, 0xDD45AAB8U, 0xBF672381U, 0x62228939U, 0x7B2231F3U, 0xA6679B4BU, 0xC4451272U, 0x1900B8CAU,
 0xF64463E6U, 0x2B01C95EU, 0x49234067U, 0x9466EADFU, 0x8D665215U, 0x5023F8ADU, 0x32017194U, 0xEF44DB2CU,
 0xE964B13DU, 0x34211B85U, 0x560392BCU, 0x8B463804U, 0x924680CEU, 0x4F032A76U, 0x2D21A34FU, 0xF06409F7U,
 0x1F20D2DBU, 0xC2657863U, 0xA047F15AU, 0x7D025BE2U, 0x6402E328U, 0xB9474990U, 0xDB65C0A9U, 0x06206A11U,
 0xD725148BU, 0x0A60BE33U, 0x6842370AU, 0xB5079DB2U, 0xAC072578U, 0x71428FC0U, 0x136006F9U, 0xCE25AC41U,
 0x2161776DU, 0xFC24DDD5U, 0x9E0654ECU, 0x4343FE54U, 0x5A43469EU, 0x8706EC26U, 0xE524651FU, 0x3861CFA7U,
 0x3E41A5B6U, 0xE3040F0EU, 0x81268637U, 0x5C632C8FU, 0x45639445U, 0x98263EFDU, 0xFA04B7C4U, 0x27411D7CU,
 0xC805C650U, 0x15406CE8U, 0x7762E5D1U, 0xAA274F69U, 0xB327F7A3U, 0x6E625D1BU, 0x0C40D422U, 0xD1057E9AU,
 0xABA65FE7U, 0x76E3F55FU, 0x14C17C66U, 0xC984D6DEU, 0xD0846E14U, 0x0DC1C4ACU, 0x6FE34D95U, 0xB2A6E72DU,
 0x5DE23C01U, 0x80A796B9U, 0xE2851F80U, 0x3FC0B538U, 0x26C00DF2U, 0xFB85A74AU, 0x99A72E73U, 0x44E284CBU,
 0x42C2EEDAU, 0x9F874462U, 0xFDA5CD5BU, 0x20E067E3U, 0x39E0DF29U, 0xE4A57591U, 0x8687FCA8U, 0x5BC25610U,
 0xB4868D3CU, 0x69C32784U, 0x0BE1AEBDU, 0xD6A40405U, 0xCFA4BCCFU, 0x12E11677U, 0x70C39F4EU, 0xAD8635F6U,
 0x7C834B6CU, 0xA1C6E1D4U, 0xC3E468EDU, 0x1EA1C255U, 0x07A17A9FU, 0xDAE4D027U, 0xB8C6591EU, 0x6583F3A6U,
 0x8AC7288AU, 0x57828232U, 0x35A00B0BU, 0xE8E5A1B3U, 0xF1E51979U, 0x2CA0B3C1U, 0x4E823AF8U, 0x93C79040U,
 0x95E7FA51U, 0x48A250E9U, 0x2A80D9D0U, 0xF7C57368U, 0xEEC5CBA2U, 0x3380611AU, 0x51A2E823U, 0x8CE7429BU,
 0x63A399B7U, 0xBEE6330FU, 0xDCC4BA36U, 0x0181108EU, 0x1881A844U, 0xC5C402FCU, 0xA7E68BC5U, 0x7AA3217DU,
 0x52A0C93FU, 0x8FE56387U, 0xEDC7EABEU, 0x30824006U, 0x2982F8CCU, 0xF4C75274U, 0x96E5DB4DU, 0x4BA071F5U,
 0xA4E4AAD9U, 0x79A10061U, 0x1B838958U, 0xC6C623E0U, 0xDFC69B2AU, 0x02833192U, 0x60A1B8ABU, 0xBDE41213U,
 0xBBC47802U, 0x6681D2BAU, 0x04A35B83U, 0xD9E6F13BU, 0xC0E649F1U, 0x1DA3E349U, 0x7F816A70U, 0xA2C4C0C8U,
 0x4D801BE4U, 0x90C5B15CU, 0xF2E73865U, 0x2FA292DDU, 0x36A22A17U, 0xEBE780AFU, 0x89C50996U, 0x5480A32EU,
 0x8585DDB4U, 0x58C0770CU, 0x3AE2FE35U, 0xE7A7548DU, 0xFEA7EC47U, 0x23E246FFU, 0x41C0CFC6U, 0x9C85657EU,
 0x73C1BE52U, 0xAE8414EAU, 0xCCA69DD3U, 0x11E3376BU, 0x08E38FA1U, 0xD5A62519U, 0xB784AC20U, 0x6AC10698U,
 0x6CE16C89U, 0xB1A4C631U, 0xD3864F08U, 0x0EC3E5B0U, 0x17C35D7AU, 0xCA86F7C2U, 0xA8A47EFBU, 0x75E1D443U,
 0x9AA50F6FU, 0x47E0A5D7U, 0x25C22CEEU, 0xF8878656U, 0xE1873E9CU, 0x3CC29424U, 0x5EE01D1DU, 0x83A5B7A5U,
 0xF90696D8U, 0x24433C60U, 0x4661B559U, 0x9B241FE1U, 0x8224A72BU, 0x5F610D93U, 0x3D4384AAU, 0xE0062E12U,
 0x0F42F53EU, 0xD2075F86U, 0xB025D6BFU, 0x6D607C07U, 0x7460C4CDU, 0xA9256E75U, 0xCB07E74CU, 0x16424DF4U,
 0x106227E5U, 0xCD278D5DU, 0xAF050464U, 0x7240AEDCU, 0x6B401616U, 0xB605BCAEU, 0xD4273597U, 0x09629F2FU,
 0xE6264403U, 0x3B63EEBBU, 0x59416782U, 0x8404CD3AU, 0x9D0475F0U, 0x4041DF48U, 0x22635671U, 0xFF26FCC9U,
 0x2E238253U, 0xF36628EBU, 0x9144A1D2U, 0x4C010B6AU, 0x5501B3A0U, 0x88441918U, 0xEA669021U, 0x37233A99U,
 0xD867E1B5U, 0x05224B0DU, 0x6700C234U, 0xBA45688CU, 0xA345D046U, 0x7E007AFEU, 0x1C22F3C7U, 0xC167597FU,
 0xC747336EU, 0x1A0299D6U, 0x782010EFU, 0xA565BA57U, 0xBC65029DU, 0x6120A825U, 0x0302211CU, 0xDE478BA4U,
 0x31035088U, 0xEC46FA30U, 0x8E647309U, 0x5321D9B1U, 0x4A21617BU, 0x9764CBC3U, 0xF54642FAU, 0x2803E842U,
};

static const vuint32 crcctbl64[256] = {
 0x00000000U, 0x38116FACU, 0x7022DF58U, 0x4833B0F4U, 0xE045BEB0U, 0xD854D11CU, 0x906761E8U, 0xA8760E44U,
 0xC5670B91U, 0xFD76643DU, 0xB545D4C9U, 0x8D54BB65U, 0x2522B521U, 0x1D33DA8DU, 0x55006A79U, 0x6D1105D5U,
 0x8F2261D3U, 0xB7330E7FU, 0xFF00BE8BU, 0xC711D127U, 0x6F67DF63U, 0x5776B0CFU, 0x1F45003BU, 0x27546F97U,
 0x4A456A42U, 0x725405EEU, 0x3A67B51AU, 0x0276DAB6U, 0xAA00D4F2U, 0x9211BB5EU, 0xDA220BAAU, 0xE2336406U,
 0x1BA8B557U, 0x23B9DAFBU, 0x6B8A6A0FU, 0x539B05A3U, 0xFBED0BE7U, 0xC3FC644BU, 0x8BCFD4BFU, 0xB3DEBB13U,
 0xDECFBEC6U, 0xE6DED16AU, 0xAEED619EU, 0x96FC0E32U, 0x3E8A0076U, 0x069B6FDAU, 0x4EA8DF2EU, 0x76B9B082U,
 0x948AD484U, 0xAC9BBB28U, 0xE4A80BDCU, 0xDCB96470U, 0x74CF6A34U, 0x4CDE0598U, 0x04EDB56CU, 0x3CFCDAC0U,
 0x51EDDF15U, 0x69FCB0B9U, 0x21CF004DU, 0x19DE6FE1U, 0xB1A861A5U, 0x89B90E09U, 0xC18ABEFDU, 0xF99BD151U,
 0x37516AAEU, 0x0F400502U, 0x4773B5F6U, 0x7F62DA5AU, 0xD714D41EU, 0xEF05BBB2U, 0xA7360B46U, 0x9F2764EAU,
 0xF236613FU, 0xCA270E93U, 0x8214BE67U, 0xBA05D1CBU, 0x1273DF8FU, 0x2A62B023U, 0x625100D7U, 0x5A406F7BU,
 0xB8730B7DU, 0x806264D1U, 0xC851D425U, 0xF040BB89U, 0x5836B5CDU, 0x6027DA61U, 0x28146A95U, 0x10050539U,
 0x7D1400ECU, 0x45056F40U, 0x0D36DFB4U, 0x3527B018U, 0x9D51BE5CU, 0xA540D1F0U, 0xED736104U, 0xD5620EA8U,
 0x2CF9DFF9U, 0x14E8B055U, 0x5CDB00A1U, 0x64CA6F0DU, 0xCCBC6149U, 0xF4AD0EE5U, 0xBC9EBE11U, 0x848FD1BDU,
 0xE99ED468U, 0xD18FBBC4U, 0x99BC0B30U, 0xA1AD649CU, 0x09DB6AD8U, 0x31CA0574U, 0x79F9B580U, 0x41E8DA2CU,
 0xA3DBBE2AU, 0x9BCAD186U, 0xD3F96172U, 0xEBE80EDEU, 0x439E009AU, 0x7B8F6F36U, 0x33BCDFC2U, 0x0BADB06EU,
 0x66BCB5BBU, 0x5EADDA17U, 0x169E6AE3U, 0x2E8F054FU, 0x86F90B0BU, 0xBEE864A7U, 0xF6DBD453U, 0xCECABBFFU,
 0x6EA2D55CU, 0x56B3BAF0U, 0x1E800A04U, 0x269165A8U, 0x8EE76BECU, 0xB6F60440U, 0xFEC5B4B4U, 0xC6D4DB18U,
 0xABC5DECDU, 0x93D4B161U, 0xDBE70195U, 0xE3F66E39U, 0x4B80607DU, 0x73910FD1U, 0x3BA2BF25U, 0x03B3D089U,
 0xE180B48FU, 0xD991DB23U, 0x91A26BD7U, 0xA9B3047BU, 0x01C50A3FU, 0x39D46593U, 0x71E7D567U, 0x49F6BACBU,
 0x24E7BF1EU, 0x1CF6D0B2U, 0x54C56046U, 0x6CD40FEAU, 0xC4A201AEU, 0xFCB36E02U, 0xB480DEF6U, 0x8C91B15AU,
 0x750A600BU, 0x4D1B0FA7U, 0x0528BF53U, 0x3D39D0FFU, 0x954FDEBBU, 0xAD5EB117U, 0xE56D01E3U, 0xDD7C6E4FU,
 0xB06D6B9AU, 0x887C0436U, 0xC04FB4C2U, 0xF85EDB6EU, 0x5028D52AU, 0x6839BA86U, 0x200A0A72U, 0x181B65DEU,
 0xFA2801D8U, 0xC2396E74U, 0x8A0ADE80U, 0xB21BB12CU, 0x1A6DBF68U, 0x227CD0C4U, 0x6A4F6030U, 0x525E0F9CU,
 0x3F4F0A49U, 0x075E65E5U, 0x4F6DD511U, 0x777CBABDU, 0xDF0AB4F9U, 0xE71BDB55U, 0xAF286BA1U, 0x9739040DU,
 0x59F3BFF2U, 0x61E2D05EU, 0x29D160AAU, 0x11C00F06U, 0xB9B60142U, 0x81A76EEEU, 0xC994DE1AU, 0xF185B1B6U,
 0x9C94B463U, 0xA485DBCFU, 0xECB66B3BU, 0xD4A70497U, 0x7CD10AD3U, 0x44C0657FU, 0x0CF3D58BU, 0x34E2BA27U,
 0xD6D1DE21U, 0xEEC0B18DU, 0xA6F30179U, 0x9EE26ED5U, 0x36946091U, 0x0E850F3DU, 0x46B6BFC9U, 0x7EA7D065U,
 0x13B6D5B0U, 0x2BA7BA1CU, 0x63940AE8U, 0x5B856544U, 0xF3F36B00U, 0xCBE204ACU, 0x83D1B458U, 0xBBC0DBF4U,
 0x425B0AA5U, 0x7A4A6509U, 0x3279D5FDU, 0x0A68BA51U, 0xA21EB415U, 0x9A0FDBB9U, 0xD23C6B4DU, 0xEA2D04E1U,
 0x873C0134U, 0xBF2D6E98U, 0xF71EDE6CU, 0xCF0FB1C0U, 0x6779BF84U, 0x5F68D028U, 0x175B60DCU, 0x2F4A0F70U,
 0xCD796B76U, 0xF56804DAU, 0xBD5BB42EU, 0x854ADB82U, 0x2D3CD5C6U, 0x152DBA6AU, 0x5D1E0A9EU, 0x650F6532U,
 0x081E60E7U, 0x300F0F4BU, 0x783CBFBFU, 0x402DD013U, 0xE85BDE57U, 0xD04AB1FBU, 0x9879010FU, 0xA0686EA3U,
};

static const vuint32 crcctbl72[256] = {
 0x00000000U, 0xEF306B19U, 0xDB8CA0C3U, 0x34BCCBDAU, 0xB2F53777U, 0x5DC55C6EU, 0x697997B4U, 0x8649FCADU,
 0x6006181FU, 0x8F367306U, 0xBB8AB8DCU, 0x54BAD3C5U, 0xD2F32F68U, 0x3DC34471U, 0x097F8FABU, 0xE64FE4B2U,
 0xC00C303EU, 0x2F3C5B27U, 0x1B8090FDU, 0xF4B0FBE4U, 0x72F90749U, 0x9DC96C50U, 0xA975A78AU, 0x4645CC93U,
 0xA00A2821U, 0x4F3A4338U, 0x7B8688E2U, 0x94B6E3FBU, 0x12FF1F56U, 0xFDCF744FU, 0xC973BF95U, 0x2643D48CU,
 0x85F4168DU, 0x6AC47D94U, 0x5E78B64EU, 0xB148DD57U, 0x370121FAU, 0xD8314AE3U, 0xEC8D8139U, 0x03BDEA20U,
 0xE5F20E92U, 0x0AC2658BU, 0x3E7EAE51U, 0xD14EC548U, 0x570739E5U, 0xB83752FCU, 0x8C8B9926U, 0x63BBF23FU,
 0x45F826B3U, 0xAAC84DAAU, 0x9E748670U, 0x7144ED69U, 0xF70D11C4U, 0x183D7ADDU, 0x2C81B107U, 0xC3B1DA1EU,
 0x25FE3EACU, 0xCACE55B5U, 0xFE729E6FU, 0x1142F576U, 0x970B09DBU, 0x783B62C2U, 0x4C87A918U, 0xA3B7C201U,
 0x0E045BEBU, 0xE13430F2U, 0xD588FB28U, 0x3AB89031U, 0xBCF16C9CU, 0x53C10785U, 0x677DCC5FU, 0x884DA746U,
 0x6E0243F4U, 0x813228EDU, 0xB58EE337U, 0x5ABE882EU, 0xDCF77483U, 0x33C71F9AU, 0x077BD440U, 0xE84BBF59U,
 0xCE086BD5U, 0x213800CCU, 0x1584CB16U, 0xFAB4A00FU, 0x7CFD5CA2U, 0x93CD37BBU, 0xA771FC61U, 0x48419778U,
 0xAE0E73CAU, 0x413E18D3U, 0x7582D309U, 0x9AB2B810U, 0x1CFB44BDU, 0xF3CB2FA4U, 0xC777E47EU, 0x28478F67U,
 0x8BF04D66U, 0x64C0267FU, 0x507CEDA5U, 0xBF4C86BCU, 0x39057A11U, 0xD6351108U, 0xE289DAD2U, 0x0DB9B1CBU,
 0xEBF65579U, 0x04C63E60U, 0x307AF5BAU, 0xDF4A9EA3U, 0x5903620EU, 0xB6330917U, 0x828FC2CDU, 0x6DBFA9D4U,
 0x4BFC7D58U, 0xA4CC1641U, 0x9070DD9BU, 0x7F40B682U, 0xF9094A2FU, 0x16392136U, 0x2285EAECU, 0xCDB581F5U,
 0x2BFA6547U, 0xC4CA0E5EU, 0xF076C584U, 0x1F46AE9DU, 0x990F5230U, 0x763F3929U, 0x4283F2F3U, 0xADB399EAU,
 0x1C08B7D6U, 0xF338DCCFU, 0xC7841715U, 0x28B47C0CU, 0xAEFD80A1U, 0x41CDEBB8U, 0x75712062U, 0x9A414B7BU,
 0x7C0EAFC9U, 0x933EC4D0U, 0xA7820F0AU, 0x48B26413U, 0xCEFB98BEU, 0x21CBF3A7U, 0x1577387DU, 0xFA475364U,
 0xDC0487E8U, 0x3334ECF1U, 0x0788272BU, 0xE8B84C32U, 0x6EF1B09FU, 0x81C1DB86U, 0xB57D105CU, 0x5A4D7B45U,
 0xBC029FF7U, 0x5332F4EEU, 0x678E3F34U, 0x88BE542DU, 0x0EF7A880U, 0xE1C7C399U, 0xD57B0843U, 0x3A4B635AU,
 0x99FCA15BU, 0x76CCCA42U, 0x42700198U, 0xAD406A81U, 0x2B09962CU, 0xC439FD35U, 0xF08536EFU, 0x1FB55DF6U,
 0xF9FAB944U, 0x16CAD25DU, 0x22761987U, 0xCD46729EU, 0x4B0F8E33U, 0xA43FE52AU, 0x90832EF0U, 0x7FB345E9U,
 0x59F09165U, 0xB6C0FA7CU, 0x827C31A6U, 0x6D4C5ABFU, 0xEB05A612U, 0x0435CD0BU, 0x308906D1U, 0xDFB96DC8U,
 0x39F6897AU, 0xD6C6E263U, 0xE27A29B9U, 0x0D4A42A0U, 0x8B03BE0DU, 0x6433D514U, 0x508F1ECEU, 0xBFBF75D7U,
 0x120CEC3DU, 0xFD3C8724U, 0xC9804CFEU, 0x26B027E7U, 0xA0F9DB4AU, 0x4FC9B053U, 0x7B757B89U, 0x94451090U,
 0x720AF422U, 0x9D3A9F3BU, 0xA98654E1U, 0x46B63FF8U, 0xC0FFC355U, 0x2FCFA84CU, 0x1B736396U, 0xF443088FU,
 0xD200DC03U, 0x3D30B71AU, 0x098C7CC0U, 0xE6BC17D9U, 0x60F5EB74U, 0x8FC5806DU, 0xBB794BB7U, 0x544920AEU,
 0xB206C41CU, 0x5D36AF05U, 0x698A64DFU, 0x86BA0FC6U, 0x00F3F36BU, 0xEFC39872U, 0xDB7F53A8U, 0x344F38B1U,
 0x97F8FAB0U, 0x78C891A9U, 0x4C745A73U, 0xA344316AU, 0x250DCDC7U, 0xCA3DA6DEU, 0xFE816D04U, 0x11B1061DU,
 0xF7FEE2AFU, 0x18CE89B6U, 0x2C72426CU, 0xC3422975U, 0x450BD5D8U, 0xAA3BBEC1U, 0x9E87751BU, 0x71B71E02U,
 0x57F4CA8EU, 0xB8C4A197U, 0x8C786A4DU, 0x63480154U, 0xE501FDF9U, 0x0A3196E0U, 0x3E8D5D3AU, 0xD1BD3623U,
 0x37F2D291U, 0xD8C2B988U, 0xEC7E7252U, 0x034E194BU, 0x8507E5E6U, 0x6A378EFFU, 0x5E8B4525U, 0xB1BB2E3CU,
};

static const vuint32 crcctbl80[256] = {
 0x00000000U, 0x68032CC8U, 0xD0065990U, 0xB8057558U, 0xA5E0C5D1U, 0xCDE3E919U, 0x75E69C41U, 0x1DE5B089U,
 0x4E2DFD53U, 0x262ED19BU, 0x9E2BA4C3U, 0xF628880BU, 0xEBCD3882U, 0x83CE144AU, 0x3BCB6112U, 0x53C84DDAU,
 0x9C5BFAA6U, 0xF458D66EU, 0x4C5DA336U, 0x245E8FFEU, 0x39BB3F77U, 0x51B813BFU, 0xE9BD66E7U, 0x81BE4A2FU,
 0xD27607F5U, 0xBA752B3DU, 0x02705E65U, 0x6A7372ADU, 0x7796C224U, 0x1F95EEECU, 0xA7909BB4U, 0xCF93B77CU,
 0x3D5B83BDU, 0x5558AF75U, 0xED5DDA2DU, 0x855EF6E5U, 0x98BB466CU, 0xF0B86AA4U, 0x48BD1FFCU, 0x20BE3334U,
 0x73767EEEU, 0x1B755226U, 0xA370277EU, 0xCB730BB6U, 0xD696BB3FU, 0xBE9597F7U, 0x0690E2AFU, 0x6E93CE67U,
 0xA100791BU, 0xC90355D3U, 0x7106208BU, 0x19050C43U, 0x04E0BCCAU, 0x6CE39002U, 0xD4E6E55AU, 0xBCE5C992U,
 0xEF2D8448U, 0x872EA880U, 0x3F2BDDD8U, 0x5728F110U, 0x4ACD4199U, 0x22CE6D51U, 0x9ACB1809U, 0xF2C834C1U,
 0x7AB7077AU, 0x12B42BB2U, 0xAAB15EEAU, 0xC2B27222U, 0xDF57C2ABU, 0xB754EE63U, 0x0F519B3BU, 0x6752B7F3U,
 0x349AFA29U, 0x5C99D6E1U, 0xE49CA3B9U, 0x8C9F8F71U, 0x917A3FF8U, 0xF9791330U, 0x417C6668U, 0x297F4AA0U,
 0xE6ECFDDCU, 0x8EEFD114U, 0x36EAA44CU, 0x5EE98884U, 0x430C380DU, 0x2B0F14C5U, 0x930A619DU, 0xFB094D55U,
 0xA8C1008FU, 0xC0C22C47U, 0x78C7591FU, 0x10C475D7U, 0x0D21C55EU, 0x6522E996U, 0xDD279CCEU, 0xB524B006U,
 0x47EC84C7U, 0x2FEFA80FU, 0x97EADD57U, 0xFFE9F19FU, 0xE20C4116U, 0x8A0F6DDEU, 0x320A1886U, 0x5A09344EU,
 0x09C17994U, 0x61C2555CU, 0xD9C72004U, 0xB1C40CCCU, 0xAC21BC45U, 0xC422908DU, 0x7C27E5D5U, 0x1424C91DU,
 0xDBB77E61U, 0xB3B452A9U, 0x0BB127F1U, 0x63B20B39U, 0x7E57BBB0U, 0x16549778U, 0xAE51E220U, 0xC652CEE8U,
 0x959A8332U, 0xFD99AFFAU, 0x459CDAA2U, 0x2D9FF66AU, 0x307A46E3U, 0x58796A2BU, 0xE07C1F73U, 0x887F33BBU,
 0xF56E0EF4U, 0x9D6D223CU, 0x25685764U, 0x4D6B7BACU, 0x508ECB25U, 0x388DE7EDU, 0x808892B5U, 0xE88BBE7DU,
 0xBB43F3A7U, 0xD340DF6FU, 0x6B45AA37U, 0x034686FFU, 0x1EA33676U, 0x76A01ABEU, 0xCEA56FE6U, 0xA6A6432EU,
 0x6935F452U, 0x0136D89AU, 0xB933ADC2U, 0xD130810AU, 0xCCD53183U, 0xA4D61D4BU, 0x1CD36813U, 0x74D044DBU,
 0x27180901U, 0x4F1B25C9U, 0xF71E5091U, 0x9F1D7C59U, 0x82F8CCD0U, 0xEAFBE018U, 0x52FE9540U, 0x3AFDB988U,
 0xC8358D49U, 0xA036A181U, 0x1833D4D9U, 0x7030F811U, 0x6DD54898U, 0x05D66450U, 0xBDD31108U, 0xD5D03DC0U,
 0x8618701AU, 0xEE1B5CD2U, 0x561E298AU, 0x3E1D0542U, 0x23F8B5CBU, 0x4BFB9903U, 0xF3FEEC5BU, 0x9BFDC093U,
 0x546E77EFU, 0x3C6D5B27U, 0x84682E7FU, 0xEC6B02B7U, 0xF18EB23EU, 0x998D9EF6U, 0x2188EBAEU, 0x498BC766U,
 0x1A438ABCU, 0x7240A674U, 0xCA45D32CU, 0xA246FFE4U, 0xBFA34F6DU, 0xD7A063A5U, 0x6FA516FDU, 0x07A63A35U,
 0x8FD9098EU, 0xE7DA2546U, 0x5FDF501EU, 0x37DC7CD6U, 0x2A39CC5FU, 0x423AE097U, 0xFA3F95CFU, 0x923CB907U,
 0xC1F4F4DDU, 0xA9F7D815U, 0x11F2AD4DU, 0x79F18185U, 0x6414310CU, 0x0C171DC4U, 0xB412689CU, 0xDC114454U,
 0x1382F328U, 0x7B81DFE0U, 0xC384AAB8U, 0xAB878670U, 0xB66236F9U, 0xDE611A31U, 0x66646F69U, 0x0E6743A1U,
 0x5DAF0E7BU, 0x35AC22B3U, 0x8DA957EBU, 0xE5AA7B23U, 0xF84FCBAAU, 0x904CE762U, 0x2849923AU, 0x404ABEF2U,
 0xB2828A33U, 0xDA81A6FBU, 0x6284D3A3U, 0x0A87FF6BU, 0x17624FE2U, 0x7F61632AU, 0xC7641672U, 0xAF673ABAU,
 0xFCAF7760U, 0x94AC5BA8U, 0x2CA92EF0U, 0x44AA0238U, 0x594FB2B1U, 0x314C9E79U, 0x8949EB21U, 0xE14AC7E9U,
 0x2ED97095U, 0x46DA5C5DU, 0xFEDF2905U, 0x96DC05CDU, 0x8B39B544U, 0xE33A998CU, 0x5B3FECD4U, 0x333CC01CU,
 0x60F48DC6U, 0x08F7A10EU, 0xB0F2D456U, 0xD8F1F89EU, 0xC5144817U, 0xAD1764DFU, 0x15121187U, 0x7D113D4FU,
};

static const vuint32 crcctbl88[256] = {
 0x00000000U, 0x493C7D27U, 0x9278FA4EU, 0xDB448769U, 0x211D826DU, 0x6821FF4AU, 0xB3657823U, 0xFA590504U,
 0x423B04DAU, 0x0B0779FDU, 0xD043FE94U, 0x997F83B3U, 0x632686B7U, 0x2A1AFB90U, 0xF15E7CF9U, 0xB86201DEU,
 0x847609B4U, 0xCD4A7493U, 0x160EF3FAU, 0x5F328EDDU, 0xA56B8BD9U, 0xEC57F6FEU, 0x37137197U, 0x7E2F0CB0U,
 0xC64D0D6EU, 0x8F717049U, 0x5435F720U, 0x1D098A07U, 0xE7508F03U, 0xAE6CF224U, 0x7528754DU, 0x3C14086AU,
 0x0D006599U, 0x443C18BEU, 0x9F789FD7U, 0xD644E2F0U, 0x2C1DE7F4U, 0x65219AD3U, 0xBE651DBAU, 0xF759609DU,
 0x4F3B6143U, 0x06071C64U, 0xDD439B0DU, 0x947FE62AU, 0x6E26E32EU, 0x271A9E09U, 0xFC5E1960U, 0xB5626447U,
 0x89766C2DU, 0xC04A110AU, 0x1B0E9663U, 0x5232EB44U, 0xA86BEE40U, 0xE1579367U, 0x3A13140EU, 0x732F6929U,
 0xCB4D68F7U, 0x827115D0U, 0x593592B9U, 0x1009EF9EU, 0xEA50EA9AU, 0xA36C97BDU, 0x782810D4U, 0x31146DF3U,
 0x1A00CB32U, 0x533CB615U, 0x8878317CU, 0xC1444C5BU, 0x3B1D495FU, 0x72213478U, 0xA965B311U, 0xE059CE36U,
 0x583BCFE8U, 0x1107B2CFU, 0xCA4335A6U, 0x837F4881U, 0x79264D85U, 0x301A30A2U, 0xEB5EB7CBU, 0xA262CAECU,
 0x9E76C286U, 0xD74ABFA1U, 0x0C0E38C8U, 0x453245EFU, 0xBF6B40EBU, 0xF6573DCCU, 0x2D13BAA5U, 0x642FC782U,
 0xDC4DC65CU, 0x9571BB7BU, 0x4E353C12U, 0x07094135U, 0xFD504431U, 0xB46C3916U, 0x6F28BE7FU, 0x2614C358U,
 0x1700AEABU, 0x5E3CD38CU, 0x857854E5U, 0xCC4429C2U, 0x361D2CC6U, 0x7F2151E1U, 0xA465D688U, 0xED59ABAFU,
 0x553BAA71U, 0x1C07D756U, 0xC743503FU, 0x8E7F2D18U, 0x7426281CU, 0x3D1A553BU, 0xE65ED252U, 0xAF62AF75U,
 0x9376A71FU, 0xDA4ADA38U, 0x010E5D51U, 0x48322076U, 0xB26B2572U, 0xFB575855U, 0x2013DF3CU, 0x692FA21BU,
 0xD14DA3C5U, 0x9871DEE2U, 0x4335598BU, 0x0A0924ACU, 0xF05021A8U, 0xB96C5C8FU, 0x6228DBE6U, 0x2B14A6C1U,
 0x34019664U, 0x7D3DEB43U, 0xA6796C2AU, 0xEF45110DU, 0x151C1409U, 0x5C20692EU, 0x8764EE47U, 0xCE589360U,
 0x763A92BEU, 0x3F06EF99U, 0xE44268F0U, 0xAD7E15D7U, 0x572710D3U, 0x1E1B6DF4U, 0xC55FEA9DU, 0x8C6397BAU,
 0xB0779FD0U, 0xF94BE2F7U, 0x220F659EU, 0x6B3318B9U, 0x916A1DBDU, 0xD856609AU, 0x0312E7F3U, 0x4A2E9AD4U,
 0xF24C9B0AU, 0xBB70E62DU, 0x60346144U, 0x29081C63U, 0xD3511967U, 0x9A6D6440U, 0x4129E329U, 0x08159E0EU,
 0x3901F3FDU, 0x703D8EDAU, 0xAB7909B3U, 0xE2457494U, 0x181C7190U, 0x51200CB7U, 0x8A648BDEU, 0xC358F6F9U,
 0x7B3AF727U, 0x32068A00U, 0xE9420D69U, 0xA07E704EU, 0x5A27754AU, 0x131B086DU, 0xC85F8F04U, 0x8163F223U,
 0xBD77FA49U, 0xF44B876EU, 0x2F0F0007U, 0x66337D20U, 0x9C6A7824U, 0xD5560503U, 0x0E12826AU, 0x472EFF4DU,
 0xFF4CFE93U, 0xB67083B4U, 0x6D3404DDU, 0x240879FAU, 0xDE517CFEU, 0x976D01D9U, 0x4C2986B0U, 0x0515FB97U,
 0x2E015D56U, 0x673D2071U, 0xBC79A718U, 0xF545DA3FU, 0x0F1CDF3BU, 0x4620A21CU, 0x9D642575U, 0xD4585852U,
 0x6C3A598CU, 0x250624ABU, 0xFE42A3C2U, 0xB77EDEE5U, 0x4D27DBE1U, 0x041BA6C6U, 0xDF5F21AFU, 0x96635C88U,
 0xAA7754E2U, 0xE34B29C5U, 0x380FAEACU, 0x7133D38BU, 0x8B6AD68FU, 0xC256ABA8U, 0x19122CC1U, 0x502E51E6U,
 0xE84C5038U, 0xA1702D1FU, 0x7A34AA76U, 0x3308D751U, 0xC951D255U, 0x806DAF72U, 0x5B29281BU, 0x1215553CU,
 0x230138CFU, 0x6A3D45E8U, 0xB179C281U, 0xF845BFA6U, 0x021CBAA2U, 0x4B20C785U, 0x906440ECU, 0xD9583DCBU,
 0x613A3C15U, 0x28064132U, 0xF342C65BU, 0xBA7EBB7CU, 0x4027BE78U, 0x091BC35FU, 0xD25F4436U, 0x9B633911U,
 0xA777317BU, 0xEE4B4C5CU, 0x350FCB35U, 0x7C33B612U, 0x866AB316U, 0xCF56CE31U, 0x14124958U, 0x5D2E347FU,
 0xE54C35A1U, 0xAC704886U, 0x7734CFEFU, 0x3E08B2C8U, 0xC451B7CCU, 0x8D6DCAEBU, 0x56294D82U, 0x1F1530A5U,
};


// conforms to RFC 3720 (section B.4.)
vuint32 crc32cBuffer (vuint32 crc32c, const void *data, size_t length) {
  if (length) {
    crc32c = ~crc32c;
    const vuint8 *src = (const vuint8 *)data;
    // handle leading misaligned bytes
    size_t initial_bytes = (sizeof(int32_t)-(intptr_t)src)&(sizeof(int32_t)-1);
    if (length < initial_bytes) initial_bytes = length;
    for (size_t f = initial_bytes; f; --f) crc32c = crcctbl32[(crc32c^*src++)&0x000000FF]^(crc32c>>8);
    // handle alighed bytes
    length -= initial_bytes;
    const size_t running_length = length&~(sizeof(vuint64)-1);
    const size_t end_bytes = length-running_length;
    for (size_t f = running_length>>3; f; --f) {
      crc32c ^= *(const vuint32 *)src;
      src += 4;
      vuint32 term1 = crcctbl88[crc32c&0x000000FF]^crcctbl80[(crc32c>>8)&0x000000FF];
      vuint32 term2 = crc32c>>16;
      crc32c = term1^crcctbl72[term2&0x000000FF]^crcctbl64[(term2>>8)&0x000000FF];
      term1 = crcctbl56[(*(const vuint32 *)src)&0x000000FF]^crcctbl48[((*(const vuint32 *)src)>>8)&0x000000FF];
      term2 = (*(const vuint32 *)src)>>16;
      crc32c = crc32c^term1^crcctbl40[term2&0x000000FF]^crcctbl32[(term2>>8)&0x000000FF];
      src += 4;
    }
    // handle trailing unaligned bytes
    for (size_t f = end_bytes; f; --f) crc32c = crcctbl32[(crc32c^*src++)&0x000000FF]^(crc32c>>8);
    crc32c = ~crc32c;
  }
  return crc32c;
}


/* test
#define CHECK(func_,val_)  do { \
  const vuint32 crc = func_(0, buf, sizeof(buf)); \
  if (crc != val_) printf("FAIL! func=" #func_ "; got: 0x%08x; expected: 0x%08x\n", crc, val_); \
  else printf("passed: " #func_ " with 0x%08x\n", val_); \
} while (0)

#define CHECK1(func_,val_)  do { \
  const vuint32 crc = func_(0, data, sizeof(data)); \
  if (crc != val_) printf("FAIL! func=" #func_ "; got: 0x%08x; expected: 0x%08x\n", crc, val_); \
  else printf("passed: " #func_ " with 0x%08x\n", val_); \
} while (0)


static void test () {
  // from rfc3720 section B.4.
  vuint8 buf[32];

  printf("testig...\n");

  memset(buf, 0, sizeof(buf));
  CHECK(calc_crc32c, 0x8a9136aaU);
  CHECK(crc32cBuffer, 0x8a9136aaU);

  memset(buf, 0xff, sizeof(buf));
  CHECK(calc_crc32c, 0x62a8ab43U);
  CHECK(crc32cBuffer, 0x62a8ab43U);

  for (size_t i = 0; i < 32; ++i) buf[i] = i&0xffu;
  CHECK(calc_crc32c, 0x46dd794eU);
  CHECK(crc32cBuffer, 0x46dd794eU);

  for (size_t i = 0; i < 32; ++i) buf[i] = (31-i)&0xffu;
  CHECK(calc_crc32c, 0x113fdb5cU);
  CHECK(crc32cBuffer, 0x113fdb5cU);

  const uint8_t data[48] = {
    0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  CHECK1(calc_crc32c, 0xd9963a56U);
  CHECK1(crc32cBuffer, 0xd9963a56U);
}
*/