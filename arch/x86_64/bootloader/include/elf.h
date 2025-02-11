// ##############################
//			  Elf.h
// ##############################

#ifndef ELF_H
#define ELF_H

// Data Types
#define Elf64_Addr unsigned long long
#define Elf64_Off unsigned long long
#define Elf64_Half unsigned short
#define Elf64_Word unsigned int
#define Elf64_Sword int 
#define Elf64_Xword unsigned long long
#define Elf64_Sxword long long

// Magic Numbers
#define Elf64_MAG0 '0x7f'
#define Elf64_MAG1 'E'
#define Elf64_MAG2 'L'
#define Elf64_MAG3 'F'

// Segments
#define PT_NULL			0
#define PT_LOAD			1
#define PT_DYNAMIC		2
#define PT_INTERP		3
#define PT_NOTE			4
#define PT_SHLIB		5
#define PT_PHDR			6
#define PT_LOOS			0x60000000
#define PT_HILOS		0x6FFFFFFF
#define PT_LOPROC		0x70000000
#define PT_HIPROC		0x7FFFFFFF

// Borrowed Definitions
#define EI_NIDENT (16)
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_PAD 8

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFMAG "\x7f""ELF"
#define SELFMAG 4

#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

#define EM_AMD64 62

#define EV_NONE 0
#define EV_CURRENT 1

#define ELFOSABI_NONE		0	/* UNIX System V ABI */
#define ELFOSABI_GNU		3	/* Object uses GNU ELF extensions.  */
#define EM_X86_64	62		/* AMD x86-64 architecture */

#define SHT_NOBITS	  8		/* Program space with no data (bss) */
#define SHF_ALLOC	     (1 << 1)	/* Occupies memory during execution */

// File Header
typedef struct
{
	unsigned char	e_ident[EI_NIDENT];		/* ELF Magic Number */
	Elf64_Half		e_type;				/* Object File Type */
	Elf64_Half		e_machine;			/* Machine Type */
	Elf64_Word		e_version;			/* Object File Version */
	Elf64_Addr		e_entry;			/* Entry Point Address */
	Elf64_Off		e_phoff;			/* Program Header Offset */
	Elf64_Off		e_shoff;			/* Section Header Offset */
	Elf64_Word		e_flags;			/* Processor Specific Flags */
	Elf64_Half		e_ehsize;			/* ELF Header Size 'REVISED FROM 'Elf64_Word'*/
	Elf64_Half		e_phentsize;		/* Program Header Entry Size */
	Elf64_Half		e_phnum;			/* Number Of Program Header Entries */
	Elf64_Half		e_shentsize;		/* Section Header Entry Size */
	Elf64_Half		e_shnum;			/* Number Of Section Header Entries */
	Elf64_Half		e_shstrndx;			/* Section Naame String Table Index */
} Elf64_Ehdr;

// Section Header
typedef struct
{
	Elf64_Word		sh_name;			/* Section Name */
	Elf64_Word		sh_type;			/* Section Type */
	Elf64_Xword		sh_flags;			/* Section Attributes */
	Elf64_Addr		sh_addr;			/* Virtual Address In Memory */
	Elf64_Off		sh_offset;			/* Section Offset In File */
	Elf64_Xword		sh_size;			/* Section Size */
	Elf64_Word		sh_link;			/* Link To Other Section */
	Elf64_Word		sh_info;			/* Misc Info */
	Elf64_Xword		sh_addralign;		/* Address Alignment Boundary */
	Elf64_Xword		sh_entsize;			/* Size of entries, if section has table */
} Elf64_Shdr;

// Program Header
typedef struct
{
	Elf64_Word		p_type;				/* Type Of Segment */
	Elf64_Word		p_flags;			/* Segment Attributes */
	Elf64_Off		p_offset;			/* Offset In File */
	Elf64_Addr		p_vaddr;			/* Virtual Address In Memory */
	Elf64_Addr		p_paddr;			/* Reserved */
	Elf64_Xword		p_filesz;			/* Size Of Segment In File */
	Elf64_Xword		p_memsz;			/* Size of Segment In Memory */
	Elf64_Xword		p_align;			/* Alignment Of Segment */
} Elf64_Phdr;

#endif
