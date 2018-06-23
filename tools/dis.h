/*
 * verbosity
 */
#define	V_BUF	0x1
#define	V_MATCH	0x2
#define	V_PARSE	0x4
#define	V_OUT	0x8
#define	V_TABLE	0x10

int bigendian;

/*
 * a format is a handle for special output forms
 */
struct format {
	char *name;
	char *prefix;
	char *suffix;
	int width;
	int flags;
#define	F_SIGNED	0x01
#define	F_HEX		0x02
#define	F_XREF		0x04
#define	F_PCREL		0x08
};
struct format *formats;
int n_formats;

/*
 * a field is a parcel in an instruction that may
 * have bound to it an array and/or a format
 */
struct field {
	char *name;
	unsigned char width;
	unsigned char bitoff;	/* least significant bit == 0 */
	unsigned char destbit;
	struct array *array;
	struct format *fmt;
	unsigned int value;
};

struct field *fields;
int n_fields;

struct operand {
	struct field *fld;
	struct operand *next;
};

#define	NUM_OPS		3
#define	NUM_VALS	3

/*
 * an iclass is a encoding form that describes an instruction
 * with variable fields
 */
struct iclass {
	unsigned long mask;
	unsigned long val;
	unsigned char ilen;

	struct operand *operand[NUM_OPS];

	char *opcode;

	struct field *opi;	  /* op name array index */
	struct array *opa;
};
struct iclass *instructions;
int n_instructions;

/* arrays of strings */
struct array {
	char *name;
	char **values;
	int count;
};
struct array *arrays;
int n_arrays;

struct array *lookup_array(char *name, int required);
struct field *lookup_field(char *name, int required);
struct format *lookup_format(char *name, int required);

extern unsigned long extract(struct field *fp, unsigned char *pc);
extern unsigned short bias(unsigned short base, unsigned short off);
extern unsigned long getlong(unsigned char *p);
extern int load_arch(char *arch);
void disas(unsigned char *destbuf, int startaddr, int size);
int decode(char *src, int size, unsigned char **dest, unsigned long *address);
extern int verbose;

/*
 * an instance of a cracked instruction
 */
struct inst {
	int addr;
	int len;

	char *op;	   /* the operation */

	struct iclass *ic;

	int vals;
	int opvals[NUM_OPS];	/* base index of op values */
	int values[NUM_VALS];
};

#define swap(a,b) { int tmp; tmp = (a); (a) = (b) ; (b) = tmp; }
#define bit_set(map, bit) ((map[(bit) / 8] & (1 << ((bit) % 8))) ? 1 : 0)
#define set_bit(map, bit) map[(bit) / 8] |= (1 << ((bit) % 8))

struct sym {
	int offset;
	int seg;
	char *name;
	struct sym *next;
};

char *getsym(int offset);

struct ref {
    char *name;
    struct refchain {
		char chained;
        int offset;
        struct refchain *next;
    } *head;
    struct ref *next;
};

char *ref(int val);

char is_code(int addr);
