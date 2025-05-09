#!/usr/bin/python3
import re

fil_desc = re.compile("^FILE_DESC")
reg_name = re.compile("^REG_NAME")
reg_addr = re.compile("^REG_ADDR")
fld_name = re.compile("^FLD_NAME")
fld_bits = re.compile("^FLD_BITS")
fld_desc = re.compile("^FLD_DESC")
end = re.compile("/// end")

desc_fname_mac = ["jm_reg_reg_file.rdf"];
desc_fname_all = desc_fname_mac;

with open("macregs.h", "w", newline = "") as h_file:
    h_file.write("#ifndef _MACREGS_H_\n#define _MACREGS_H_\n")
    for fname in desc_fname_all:
        try:
            with open(fname) as src_file:
                macro = ""
                for line in src_file:
                    value = line.partition("=")[2]
                    line_to_write = ""
                    if fil_desc.match(line):
                        line_to_write = "/*\n * " + value.strip() + "\n */\n"
                    elif reg_name.match(line):
                        rname = value.strip().upper()
                    elif reg_addr.match(line):
                        raddr = value.lstrip()
                        raddr = "0x" + raddr[-4:]
                        line_to_write = f"\n#define {rname:68}{raddr:>11}"
                    elif fld_name.match(line):
                        fname = rname[4:] + "_" + value.strip().upper()
                    elif fld_bits.match(line):
                        fbits = value.strip().strip('[]')
                        pos_b = int(fbits.partition(":")[2].strip())
                        pos_e = int(fbits.partition(":")[0].strip())
                        mask = 0
                        for p in range(pos_e - pos_b + 1):
                            mask |= 1 << p
                        mask <<= pos_b
                        fmask = fname + "_MASK"
                        fshif = fname + "_SHIFT"
                        macro = f"#define     {fmask:64}{hex(mask):>10}\n" \
                                + f"#define     {fshif:64}{pos_b:>10}\n"
                    elif fld_desc.match(line):
                        line_to_write = "/* " + value.strip() + " */\n"
                    elif end.match(line):
                        line = ""
                    else:
                        continue
                    if line_to_write:
                        h_file.write(line_to_write)
                    if macro:
                        if fld_desc.match(line) == None \
                                and fld_bits.match(line) == None:
                            h_file.write(macro)
                            macro = ""
        except FileNotFoundError:
            print("File \"" + fname + "\" not found!!")
    h_file.write(r"#endif /*_MACREGS_H_*/")
