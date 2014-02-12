#include <stdio.h>
#include <elf.h>
#include <string.h>

char source[1024];

void readSource(FILE *in)
{
  int i = 0;
  while (1) {
    char ch = fgetc(in);
    if (ch <= 0)
      break;
    source[i++] = ch;
  }
  source[i] = 0;
}

char staticData[20480];
int dataPtr = 0;

typedef struct {
  int len;
  int dataOfs;
} DataPtr;

DataPtr bottlesPtr;
DataPtr quinePtr;
DataPtr helloPtr;

void outputData(const char *str, DataPtr *data)
{
  int len = strlen(str);
  data->len = len;
  data->dataOfs = dataPtr;
  memcpy(&staticData[dataPtr], str, len);
  dataPtr += len;
}

DataPtr *bottlesOfBeer()
{
  if (bottlesPtr.len <= 0) {
    char buf[20480];
    char line[1024];
    int i;
    buf[0] = 0;
    for (i = 99; i > 1; i--) {
      sprintf(line, "%d bottles of beer on the wall, %d bottles of beer.\n"
                    "Take one down and pass it around, %d bottle%s of beer on the wall.\n\n", i, i, i - 1, i > 2 ? "s" : "");
      strcat(buf, line);
    }
    sprintf(line, "1 bottle of beer on the wall, 1 bottle of beer.\n"
                  "Take one down and pass it around, no more bottles of beer on the wall.\n\n");
    strcat(buf, line);
    sprintf(line, "No more bottles of beer on the wall, no more bottles of beer.\n"
                  "Go to the store and buy some more, 99 bottles of beer on the wall.\n");
    strcat(buf, line);
    outputData(buf, &bottlesPtr);
  }
  return &bottlesPtr;
}

DataPtr *quine()
{
  if (quinePtr.len <= 0) {
    outputData(source, &quinePtr);
  }
  return &quinePtr;
}

DataPtr *helloWorld()
{
  if (helloPtr.len <= 0) {
    outputData("Hello world!\n", &helloPtr);
  }
  return &helloPtr;
}

char code[10240];
int codePtr = 0;

typedef struct {
  int codeOfs;
  int dataOfs;
} RefPtr;
RefPtr relocations[1024];
int relocationsCount = 0;

int outputCode(char instr[], int codeSize)
{
  int ofs = codePtr;
  memcpy(&code[codePtr], instr, codeSize);
  codePtr += codeSize;
  return ofs;
}

void outputPrint(DataPtr *data)
{
  char code[] = {
    0xBA, 0x00, 0x00, 0x00, 0x00, // mov edx, length
    0xB9, 0x00, 0x00, 0x00, 0x00, // mov ecx, dataPtr
    0xBB, 0x01, 0x00, 0x00, 0x00, // mov ebx, 1
    0xB8, 0x04, 0x00, 0x00, 0x00, // mov eax, 4
    0xCD, 0x80                    // int 0x80
  };
  *((int *) &code[1]) = data->len;
  int p = outputCode(code, sizeof(code));

  RefPtr *ptr = &relocations[relocationsCount++];
  ptr->codeOfs = p + 6;
  ptr->dataOfs = data->dataOfs;
}

int parse()
{
  char *ptr = source;
  while (1) {
    char ch = *ptr++;
    if (ch == 0)
      break;
    if (ch == 'h' || ch == 'H') {
      outputPrint(helloWorld());
    } else if (ch == 'q' || ch == 'Q') {
      outputPrint(quine());
    } else if (ch == '9') {
      outputPrint(bottlesOfBeer());
    } else if (ch > ' ') {
      fprintf(stderr, "Error: unexpected char %c\n", ch);
      return 0;
    }
  }
  char epilog[] = {
    0xB8, 0x01, 0x00, 0x00, 0x00, // mov eax, 1
    0xBB, 0x2A, 0x00, 0x00, 0x00, // mov ebx, 42
    0xCD, 0x80                    // int 0x80
  };
  outputCode(epilog, sizeof(epilog));
  return 1;
}

void relocate(int base)
{
  int i;
  for (i = 0; i < relocationsCount; i++) {
    RefPtr *ptr = &relocations[i];
    *((int *) &code[ptr->codeOfs]) = ptr->dataOfs + base;
  }
}

int outputElfHeaders(int codeSize, int dataSize, FILE *f)
{
  const int start = 0x8048000;

  Elf32_Ehdr header;
  Elf32_Phdr ph1;
  Elf32_Phdr ph2;

  memset(header.e_ident, 0, sizeof(header.e_ident));
  header.e_ident[EI_MAG0] = ELFMAG0;
  header.e_ident[EI_MAG1] = ELFMAG1;
  header.e_ident[EI_MAG2] = ELFMAG2;
  header.e_ident[EI_MAG3] = ELFMAG3;

  header.e_ident[EI_CLASS] = ELFCLASS32;
  header.e_ident[EI_DATA] = ELFDATA2LSB;
  header.e_ident[EI_VERSION] = EV_CURRENT;
  header.e_ident[EI_OSABI] = ELFOSABI_SYSV;
  
  header.e_type = ET_EXEC;
  header.e_machine = EM_386;
  header.e_version = EV_CURRENT;
  header.e_entry = start + sizeof(header) + sizeof(ph1) + sizeof(ph2);
  header.e_phoff = sizeof(header);
  header.e_shoff = 0;
  header.e_flags = 0;
  header.e_ehsize = sizeof(header);
  header.e_phentsize = sizeof(Elf32_Phdr);
  header.e_phnum = 2;
  header.e_shentsize = 0;
  header.e_shnum = 0;
  header.e_shstrndx = 0;

  ph1.p_type = PT_LOAD;
  ph1.p_offset = sizeof(header) + sizeof(ph1) + sizeof(ph2);
  ph1.p_vaddr = start + ph1.p_offset;
  ph1.p_paddr = 0;
  ph1.p_filesz = ph1.p_memsz = codeSize;
  ph1.p_flags = PF_R | PF_X;
  ph1.p_align = 0x1000;

  ph2.p_type = PT_LOAD;
  ph2.p_offset = sizeof(header) + sizeof(ph1) + sizeof(ph2) + codeSize;
  ph2.p_vaddr = start + ph2.p_offset;
  ph2.p_paddr = 0;
  ph2.p_filesz = ph2.p_memsz = dataSize;
  ph2.p_flags = PF_R;
  ph2.p_align = 0x1000;

  fwrite(&header, sizeof(header), 1, f);
  fwrite(&ph1, sizeof(ph1), 1, f);
  fwrite(&ph2, sizeof(ph2), 1, f);

  return ph2.p_vaddr;
}

void outputProgramCode(char code[], int codeSize,
                       char data[], int dataSize, 
                       FILE *f)
{
  fwrite(code, codeSize, 1, f);
  fwrite(data, dataSize, 1, f);
}

int main(int argc, char *argv[])
{
  FILE *in;
  char outName[1024];
  strcpy(outName, "hq9.out");
  char *inName;
  if (argc > 1) {
    inName = argv[1];
    in = fopen(inName, "rt");
    if (in == NULL) {
      fprintf(stderr, "Cannot open %s\n", inName);
      return 1;
    }
    char *p = strrchr(inName, '.');
    if (p) {
      int len = p - inName;
      memcpy(outName, inName, len);
      outName[len] = 0;
    }
  } else {
    inName = NULL;
    in = stdin;
  }
  readSource(in);
  if (inName) {
    fclose(in);
  }
  if (!parse())
    return 1;
  
  FILE *out = fopen(outName, "w+b");
  int dataBase = outputElfHeaders(codePtr, dataPtr, out);
  relocate(dataBase);
  outputProgramCode(code, codePtr, staticData, dataPtr, out);
  fclose(out);
  
  return 0;
}
