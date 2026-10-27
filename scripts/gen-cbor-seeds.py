#!/usr/bin/env python3
"""gen-cbor-seeds.py — materializes the RFC 8949 Appendix A vectors and
Appendix F rejects into test/fuzz/corpus/cbor/ (the fuzz seed corpus,
TODO.cbor/06). Provenance: the vectors are normative text of RFC 8949."""
import pathlib
import sys

VECTORS = {
    # Appendix A (Table 6): diagnostic -> hex
    "i0": "00", "i1": "01", "i10": "0a", "i23": "17", "i24": "1818", "i25": "1819",
    "i100": "1864", "i1000": "1903e8", "i1e6": "1a000f4240", "i1e12": "1b000000e8d4a51000",
    "u64max": "1bffffffffffffffff", "bignum2p64": "c249010000000000000000",
    "neg2p64": "3bffffffffffffffff", "negbignum": "c349010000000000000000",
    "n1": "20", "n10": "29", "n100": "3863", "n1000": "3903e7",
    "f0": "f90000", "fneg0": "f98000", "f1": "f93c00", "f11": "fb3ff199999999999a",
    "f15": "f93e00", "f65504": "f97bff", "f100000": "fa47c35000", "fmax32": "fa7f7fffff",
    "f1e300": "fb7e37e43c8800759c", "fsub": "f90001", "f2pneg14": "f90400", "fn4": "f9c400",
    "fn41": "fbc010666666666666", "inf_h": "f97c00", "nan_h": "f97e00", "ninf_h": "f9fc00",
    "inf_s": "fa7f800000", "nan_s": "fa7fc00000", "ninf_s": "faff800000",
    "inf_d": "fb7ff0000000000000", "nan_d": "fb7ff8000000000000",
    "ninf_d": "fbfff0000000000000",
    "false": "f4", "true": "f5", "null": "f6", "undefined": "f7", "simple16": "f0",
    "simple255": "f8ff",
    "tag0time": "c074323031332d30332d32315432303a30343a30305a", "tag1int": "c11a514b67b0",
    "tag1float": "c1fb41d452d9ec200000", "tag23bytes": "d74401020304",
    "tag24bytes": "d818456449455446",
    "tag32uri": "d82076687474703a2f2f7777772e6578616d706c652e636f6d",
    "bytes_empty": "40", "bytes01020304": "4401020304", "str_empty": "60", "stra": "6161",
    "strIETF": "6449455446", "stresc": "62225c", "stru_": "62c3bc", "strwater": "63e6b0b4",
    "strgreek": "64f0908591",
    "arr_empty": "80", "arr123": "83010203", "arrnested": "8301820203820405",
    "arr25": "98190102030405060708090a0b0c0d0e0f101112131415161718181819",
    "map_empty": "a0", "mapintkeys": "a201020304", "mapab": "a26161016162820203",
    "arrmap": "826161a161626163", "map5": "a56161614161626142616361436164614461656145",
    "indef_bytes": "5f42010243030405ff", "indef_text": "7f657374726561646d696e67ff",
    "indef_arr_empty": "9fff", "indef_arr1": "9f018202039f0405ffff",
    "indef_arr2": "9f01820203820405ff", "indef_arr3": "83018202039f0405ff",
    "indef_arr4": "83019f0203ff820405",
    "indef_arr25": "9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff",
    "indef_map1": "bf61610161629f0203ffff", "indef_map2": "826161bf61626163ff",
    "indef_map3": "bf6346756ef563416d7421ff",
    # Appendix F.1 reject corpus (representative classes)
    "rej_head1": "18", "rej_head2": "1901", "rej_head3": "1b010203040506",
    "rej_str_short": "5bffffffffffffffff010203", "rej_arr_unclosed": "81818181818181",
    "rej_map_unclosed": "a20000", "rej_tag_nocontent": "c0",
    "rej_indef_str": "5f4100", "rej_indef_arr": "9f0102",
    "rej_reserved1": "1c", "rej_reserved2": "5e", "rej_simple2b": "f800",
    "rej_chunk_type": "5f00ff", "rej_chunk_indef": "5f5f4100ffff",
    "rej_break_alone": "ff", "rej_break_arr": "81ff", "rej_break_mapval": "bf00ff",
    "rej_mt31": "1f", "rej_toomuch": "0102",
}

def main() -> int:
    out = pathlib.Path(__file__).resolve().parent.parent / "test/fuzz/corpus/cbor"
    out.mkdir(parents=True, exist_ok=True)
    for name, hexstr in VECTORS.items():
        (out / f"{name}.cbor").write_bytes(bytes.fromhex(hexstr))
    print(f"wrote {len(VECTORS)} seeds to {out}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
