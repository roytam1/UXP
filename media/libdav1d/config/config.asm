; Mozilla build configuration for bundled dav1d x86 assembly.

%define private_prefix dav1d

%ifndef ARCH_X86_64
    %ifidn __OUTPUT_FORMAT__,elf64
        %define ARCH_X86_64 1
    %elifidn __OUTPUT_FORMAT__,macho64
        %define ARCH_X86_64 1
    %elifidn __OUTPUT_FORMAT__,win64
        %define ARCH_X86_64 1
    %elifidn __OUTPUT_FORMAT__,x64
        %define ARCH_X86_64 1
    %else
        %define ARCH_X86_64 0
    %endif
%endif

%ifndef ARCH_X86_32
    %if ARCH_X86_64
        %define ARCH_X86_32 0
    %else
        %define ARCH_X86_32 1
    %endif
%endif

%ifndef STACK_ALIGNMENT
    %if ARCH_X86_64
        %define STACK_ALIGNMENT 16
    %elifidn __OUTPUT_FORMAT__,elf
        %define STACK_ALIGNMENT 16
    %elifidn __OUTPUT_FORMAT__,elf32
        %define STACK_ALIGNMENT 16
    %elifidn __OUTPUT_FORMAT__,macho
        %define STACK_ALIGNMENT 16
    %elifidn __OUTPUT_FORMAT__,macho32
        %define STACK_ALIGNMENT 16
    %else
        %define STACK_ALIGNMENT 4
    %endif
%endif

%ifndef PIC
    %ifidn __OUTPUT_FORMAT__,win32
        %define PIC 0
    %else
        %define PIC 1
    %endif
%endif

%ifndef FORCE_VEX_ENCODING
    %define FORCE_VEX_ENCODING 0
%endif

%ifidn __OUTPUT_FORMAT__,win32
    %if ARCH_X86_32
        %define PREFIX 1
    %endif
%elifidn __OUTPUT_FORMAT__,macho
    %define PREFIX 1
%elifidn __OUTPUT_FORMAT__,macho32
    %define PREFIX 1
%elifidn __OUTPUT_FORMAT__,macho64
    %define PREFIX 1
%endif
