# wlp4-compiler

The compiler works in the following steps:

1. Scanning: The **scanner** scans and tokenizes the code
2. Parsing: constructing a parse tree from the scanned tokens. Check for syntax and semantics.
3. Type-checking: annotate the parse tree with types and check for type correctness.
4. Codegen: generate MIPS assembly code from the annotated parse tree.

## Directories with respect to steps

scanner -> parse -> type -> codegen
