#include "polipo.h"

/* Atoms are interned, read-only reference-counted strings.

   Interned means that equality of atoms is equivalent to structural
   equality -- you don't need to strcmp, you just compare the AtomPtrs.
   This property is used throughout Polipo, e.g. to speed up the HTTP
   parser.

   Polipo's atoms may contain NUL bytes -- you can use internAtomN to
   store any random binary data within an atom.  However, Polipo always
   terminates your data, so if you store textual data in an atom, you
   may use the result of atomString as though it were a (read-only)
   C string.
*/

static AtomPtr *atomHashTable;
int used_atoms; //使用的atom个数
//change
//ref: main.c
void
initAtoms()
{
	//calloc： http://blog.csdn.net/zhongjiekangping/article/details/6162748
	//1 << LOG2_ATOM_HASH_TABLE_SIZE) = 2^10 = 1024
    atomHashTable = calloc((1 << LOG2_ATOM_HASH_TABLE_SIZE), sizeof(AtomPtr)); //数组链表 hash表
    
    if(atomHashTable == NULL) {
        do_log(L_ERROR, "Couldn't allocate atom hash table.\n");
        exit(1);
    }
    used_atoms = 0;
}

//internAtomN 和 internAtom 向hash表填充内容
AtomPtr
internAtomN(const char *string, int n)
{
    AtomPtr atom;
    int h;

    if(n < 0 || n >= (1 << (8 * sizeof(unsigned short))))
        return NULL;

    h = hash(0, string, n, LOG2_ATOM_HASH_TABLE_SIZE); //log2（2014） = 10
    atom = atomHashTable[h]; // 0 <= h < 1024
    while(atom) {
        if(atom->length == n &&
           (n == 0 || memcmp(atom->string, string, n) == 0)) //空字符串或者和要存的字符串前n个元素相同 //找到了 结束查询
            break;
        atom = atom->next;
    }

	//atom指向最后面
    if(!atom) { //hash表中没有存放该string， 将string存在最后面，并设置引用次数refcount为0
        atom = malloc(sizeof(AtomRec) - 1 + n + 1);
        if(atom == NULL) { //分配内存失败
            return NULL;
        }
        atom->refcount = 0;
        atom->length = n;
        /* Atoms are used both for binary data and strings.  To make
           their use as strings more convenient, atoms are always
           NUL-terminated. */
        memcpy(atom->string, string, n);
        atom->string[n] = '\0';
        atom->next = atomHashTable[h]; //放到上面
        atomHashTable[h] = atom; //第h个位置
        used_atoms++;
    }
    do_log(D_ATOM_REFCOUNT, "A 0x%lx %d++\n",
           (unsigned long)atom, atom->refcount);
    atom->refcount++; //该atom引用次数加1
    return atom;
}

AtomPtr
internAtom(const char *string)
{
    return internAtomN(string, strlen(string));
}

/**
 * 追加string到atom->string中
 * ref: diskcaches.c
 */
AtomPtr
atomCat(AtomPtr atom, const char *string)
{
    char buf[128];
    char *s = buf;
    AtomPtr newAtom;
    int n = strlen(string);
    if(atom->length + n > 128) { //查过了buf的缓存的长度
        s = malloc(atom->length + n + 1);
        if(s == NULL)
            return NULL;
    }
    memcpy(s, atom->string, atom->length); //把原来的存进去buff
    memcpy(s + atom->length, string, n); //追加新的
    newAtom = internAtomN(s, atom->length + n);
    if(s != buf) free(s); //追加后的string长度大于了缓存buff的大小
    return newAtom;
}

/**
 * 以字符c分割atom->string,存储到return1和return2
 */
int
atomSplit(AtomPtr atom, char c, AtomPtr *return1, AtomPtr *return2)
{
    char *p;
    AtomPtr atom1, atom2;
    p = memchr(atom->string, c, atom->length); //返回c在string中位置

	if(p == NULL) //未找到
        return 0;

	atom1 = internAtomN(atom->string, p - atom->string); //前p个组成一个
    if(atom1 == NULL)
        return -ENOMEM;

    atom2 = internAtomN(p + 1, atom->length - (p + 1 - atom->string));
	if(atom2 == NULL) {
        releaseAtom(atom1); //2失败了，把1也清空
        return -ENOMEM;
    }

    *return1 = atom1;
    *return2 = atom2;
    return 1;
}

/**
 * 将前n个元素设置为小写的string到atom中
 */
AtomPtr
internAtomLowerN(const char *string, int n)
{
    char *s;
    char buf[100];
    AtomPtr atom;

    if(n < 0 || n >= 50000)
        return NULL;

    if(n < 100) {
        s = buf;
    } else {
        s = malloc(n);
        if(s == NULL)
            return NULL;
    }

    lwrcpy(s, string, n);
    atom = internAtomN(s, n);
    if(s != buf) free(s);
    return atom;
}

/**
 * 多引用一次， 就是保留多1次， 不要删除
 */
AtomPtr
retainAtom(AtomPtr atom)
{
    if(atom == NULL)
        return NULL;

    do_log(D_ATOM_REFCOUNT, "A 0x%lx %d++\n",
           (unsigned long)atom, atom->refcount);
    assert(atom->refcount >= 1 && atom->refcount < LARGE_ATOM_REFCOUNT); //未达到最大引用次数
    atom->refcount++;
    return atom;
}

/**
 * Atom被引用次数为0时，释放，否则不释放， 引用次数减1
 * 没有引用次数的时候，将该元素的删掉，并同时设置used_atoms减一
 */
void
releaseAtom(AtomPtr atom)
{
	//为了将一个函数写成独立的功能， 就要不考虑传进的参数一定是什么，一定不为NULL之类的
	//在函数里考虑所有可能的情况，站在参数的角度，而不是调用关系
	//视角只放在这个函数上，不要考虑其他的外部因素
    if(atom == NULL)
        return;

    do_log(D_ATOM_REFCOUNT, "A 0x%lx %d--\n",
           (unsigned long)atom, atom->refcount); //记录释放前引用次数
    assert(atom->refcount >= 1 && atom->refcount < LARGE_ATOM_REFCOUNT); //范围限制

    atom->refcount--;

    if(atom->refcount == 0) { //当前元素没有引用了，就删除该元素
        int h = hash(0, atom->string, atom->length, LOG2_ATOM_HASH_TABLE_SIZE);
        assert(atomHashTable[h] != NULL); //随时想着判断错误情况

        if(atom == atomHashTable[h]) { //atom等于第一个元素，删除atom
            atomHashTable[h] = atom->next;
            free(atom);
        } else {  //查找
			//单链表： 为了删除当前元素，需要查找它之前的一个元素
            AtomPtr previous = atomHashTable[h];
            while(previous->next) {
                if(previous->next == atom)
                    break;
                previous = previous->next;
            }
            assert(previous->next != NULL);
            previous->next = atom->next;
            free(atom);
        }
        used_atoms--;
    }
}

/**
 * 格式化的形式初始化atom->string 像c语言的printf("%d,%s", 2, "43");
 * internAtomF("%d,%s", 2, "43")
 */
AtomPtr
internAtomF(const char *format, ...)
{
    char *s;
    char buf[150];
    int n;
    va_list args;
    AtomPtr atom = NULL;

    va_start(args, format);
    n = vsnprintf(buf, 150, format, args);
    va_end(args);
    if(n >= 0 && n < 150) {
        atom = internAtomN(buf, n);
    } else {
        va_start(args, format);
        s = vsprintf_a(format, args);
        va_end(args);
        if(s != NULL) {
            atom = internAtom(s);
            free(s);
        }
    }
    return atom;
}

//存储e对应的错误字符串 和 f格式化后的字符串
static AtomPtr
internAtomErrorV(int e, const char *f, va_list args)
{
    char *es = pstrerror(e);
    AtomPtr atom;
    char *s1, *s2;
    int n, rc;
    va_list args_copy;

    if(f) {
        va_copy(args_copy, args);
        s1 = vsprintf_a(f, args_copy);
        va_end(args_copy);
        if(s1 == NULL)
            return NULL;
        n = strlen(s1);
    } else {
        s1 = NULL;
        n = 0;
    }

    s2 = malloc(n + 70);
    if(s2 == NULL) {
        free(s1);
        return NULL;
    }
    if(s1) {
        strcpy(s2, s1);
        free(s1);
    }

    rc = snprintf(s2 + n, 69, f ? ": %s" : "%s", es);
    if(rc < 0 || rc >= 69) { //最长70的字符
        free(s2);
        return NULL;
    }

    atom = internAtomN(s2, n + rc);
    free(s2);
    return atom;
}

AtomPtr
internAtomError(int e, const char *f, ...)
{
    AtomPtr atom;
    va_list args;
    va_start(args, f);
    atom = internAtomErrorV(e, f, args);
    va_end(args);
    return atom;
}

//提取string数组
char *
atomString(AtomPtr atom)
{
    if(atom)
        return atom->string;
    else
        return "(null)";
}

//下面的函数 针对AtomListPtr结构体来操作
/**
 * 将长度为n数组变成链表
 */
AtomListPtr
makeAtomList(AtomPtr *atoms, int n) // 0<n<1024
{
	//这里灭有判断 atoms 是否为NULL，而是间接的通过判断n是否等于0。
    AtomListPtr list;
    list = malloc(sizeof(AtomListRec));
    if(list == NULL) return NULL;
    list->length = 0;
    list->size = 0;
    list->list = NULL;

	if(n > 0) {
        int i;
        list->list = malloc(n * sizeof(AtomPtr));

		if(list->list == NULL) {
            free(list);
            return NULL;
        }
        list->size = n;
        for(i = 0; i < n; i++)
            list->list[i] = atoms[i]; //只是传递了引用，并没有复制数据
        list->length = n;
    }
    return list;
}

/**
 * 销毁链表
 */
void
destroyAtomList(AtomListPtr list)
{
	//没有判断list是否为NULL，不科学
    int i;
    if(list->list) {
        for(i = 0; i < list->length; i++)
            releaseAtom(list->list[i]); //还要查看list-list[i]的refcount是否等于0
        list->length = 0;
        free(list->list);
        list->list = NULL;
        list->size = 0;
    }
    assert(list->size == 0);
    free(list);
}

/**
 * 判断atom是否是list的一个成员
 */
int
atomListMember(AtomPtr atom, AtomListPtr list)
{
    int i;
    for(i = 0; i < list->length; i++) {
        if(atom == list->list[i])
            return 1;
    }
    return 0;
}

/**
 * 将atom追加到list的最后面
 */
void
atomListCons(AtomPtr atom, AtomListPtr list)
{
    if(list->list == NULL) {
        assert(list->size == 0);
        list->list = malloc(5 * sizeof(AtomPtr)); //初始化size=5
        if(list->list == NULL) {
            do_log(L_ERROR, "Couldn't allocate AtomList\n");
            return;
        }
        list->size = 5;
    }
    //内存不够 再申请2倍的
    if(list->size <= list->length) {
        AtomPtr *new_list;
        int n = (2 * list->length + 1);
        new_list = realloc(list->list, n * sizeof(AtomPtr));
        if(new_list == NULL) {
            do_log(L_ERROR, "Couldn't realloc AtomList\n");
            return;
        }
        list->list = new_list;
        list->size = n;
    }
    list->list[list->length] = atom;
    list->length++;
}
