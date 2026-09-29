#!/usr/bin/env python3

NUM = 256

# -------------------------
# 1. extern declarations
# -------------------------
print("/* extern ISR declarations */")
for i in range(NUM):
    print(f"extern void isr{i}(void);")

print("\n/* ISR table */")
print("static void (*isr_stubs[256])(void) = {")

# -------------------------
# 2. table initialization
# -------------------------
for i in range(NUM):
    if i == NUM - 1:
        print(f"    isr{i}")
    else:
        print(f"    isr{i},")

print("};")
