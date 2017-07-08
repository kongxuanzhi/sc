#undef MAX
#undef MIN

#define MAX(x,y) ((x)<=(y)?(y):(x))
#define MIN(x,y) ((x)<=(y)?(x):(y))

struct _HTTPRequest;

#if defined(USHRT_MAX) && CHUNK_SIZE <= USHRT_MAXRequestFunction
typedef unsigned short chunk_size_t;
#else
typedef unsigned int chunk_size_t;
#endif

//块
typedef struct _Chunk {
    short int locked; //块被锁定了吗
    chunk_size_t size; //块大小
    char *data; //块的内容
} ChunkRec, *ChunkPtr;

struct _Object;

//函数指针
typedef int (*RequestFunction)(struct _Object *, int, int, int,
                               struct _HTTPRequest*, void*);

typedef struct _Object {
    short refcount;
    unsigned char type; //OBJECT_HTTP ｜　OBJECT＿DNS
    RequestFunction request;
    void *request_closure;
    char *key;
    unsigned short key_size;
    unsigned short flags; //90行
    unsigned short code;
    void *abort_data;
    struct _Atom *message;
    int length;
    time_t date;
    time_t age;
    time_t expires;
    time_t last_modified;
    time_t atime;
    char *etag;
    unsigned short cache_control;
    int max_age;
    int s_maxage;
    struct _Atom *headers;
    struct _Atom *via;
    int size;
    int numchunks;
    ChunkPtr chunks;
    void *requestor;
    struct _Condition condition;
    struct _DiskCacheEntry *disk_entry;
    struct _Object *next, *previous;
} ObjectRec, *ObjectPtr;

typedef struct _CacheControl {
    int flags;
    int max_age;
    int s_maxage;
    int min_fresh;
    int max_stale;
} CacheControlRec, *CacheControlPtr;

extern int cacheIsShared;
extern int mindlesslyCacheVary;

extern CacheControlRec no_cache_control;
extern int objectExpiryScheduled;
extern int publicObjectCount;
extern int privateObjectCount;
extern int idleTime;

extern const time_t time_t_max;

extern int publicObjectLowMark, objectHighMark;

extern int log2ObjectHashTableSize;

//定义两种OBJECT TYPE
/* object->type */
#define OBJECT_HTTP 1
#define OBJECT_DNS 2
//初始化对象需要将headers填充 struct _Atom *headers;
//当对象正在被服务器使用时，设置flag为OBJECT_INPROGRESS
/* object->flags */
/* object is public */
#define OBJECT_PUBLIC 1 //公开的
/* object hasn't got any headers yet */
#define OBJECT_INITIAL 2 //未初始化
/* a server connection is already taking care of the object */
#define OBJECT_INPROGRESS 4 //正在使用中
/* the object has been superseded -- don't try to fetch it */
#define OBJECT_SUPERSEDED 8 //对象会被取代
/* the object is private and aditionally can only be used by its requestor */
#define OBJECT_LINEAR 16 //对象是私有的，并且只能被请求者访问
/* the object is currently being validated */
#define OBJECT_VALIDATING 32 //对象正在被验证
/* object has been aborted */ 
#define OBJECT_ABORTED 64 //对象已经被终止了
/* last object request was a failure */
#define OBJECT_FAILED 128 //最后请求对象失败
/* Object is a local file */ 
#define OBJECT_LOCAL 256 //对象是一个本地文件
/* The object's data has been entirely written out to disk */
#define OBJECT_DISK_ENTRY_COMPLETE 512 //对象已经被完全写入磁盘
/* The object is suspected to be dynamic -- don't PMM */
#define OBJECT_DYNAMIC 1024 //对象可能是动态的
/* Used for synchronisation between client and server. */
#define OBJECT_MUTATING 2048 //跟客户端和服务端的同步有关系

/* object->cache_control and connection->cache_control */
/* RFC 2616 14.9 */
/* Non-standard: like no-cache, but kept internally */
#define CACHE_NO_HIDDEN 1 //没有缓存，保持内部
/* no-cache */
#define CACHE_NO 2 //没有缓存
/* public */
#define CACHE_PUBLIC 4 //公共缓存
/* private */
#define CACHE_PRIVATE 8 //私有缓存
/* no-store */
#define CACHE_NO_STORE 16 //
/* no-transform */
#define CACHE_NO_TRANSFORM 32
/* must-revalidate */
#define CACHE_MUST_REVALIDATE 64 //一定重新验证
/* proxy-revalidate */
#define CACHE_PROXY_REVALIDATE 128 //代理重新验证
/* only-if-cached */
#define CACHE_ONLY_IF_CACHED 256 //有缓存的时候，才会使用
/* set if Vary header; treated as no-cache */
#define CACHE_VARY 512  //不同的头，视为没有缓存
/* set if Authorization header; treated specially */
#define CACHE_AUTHORIZATION 1024 //设置授权头，特别对待
/* set if cookie */
#define CACHE_COOKIE 2048 //如果有cookie设置
/* set if this object should never be combined with another resource */
#define CACHE_MISMATCH 4096 //对象不与其他资源组合

struct _HTTPRequest;

void preinitObject(void);
void initObject(void);
ObjectPtr findObject(int type, const void *key, int key_size);
ObjectPtr makeObject(int type, const void *key, int key_size,
                     int public, int fromdisk,
                     int (*request)(ObjectPtr, int, int, int, 
                                    struct _HTTPRequest*, void*), void*);
void objectMetadataChanged(ObjectPtr object, int dirty);
ObjectPtr retainObject(ObjectPtr);
void releaseObject(ObjectPtr);
int objectSetChunks(ObjectPtr object, int numchunks);
void lockChunk(ObjectPtr, int);
void unlockChunk(ObjectPtr, int);
void destroyObject(ObjectPtr object);
void privatiseObject(ObjectPtr object, int linear);
void abortObject(ObjectPtr object, int code, struct _Atom *message);
void supersedeObject(ObjectPtr);
void notifyObject(ObjectPtr);
void releaseNotifyObject(ObjectPtr);
ObjectPtr objectPartial(ObjectPtr object, int length, struct _Atom *headers);
int objectHoleSize(ObjectPtr object, int offset)
    ATTRIBUTE ((pure));
int objectHasData(ObjectPtr object, int from, int to)
    ATTRIBUTE ((pure));
int objectAddData(ObjectPtr object, const char *data, int offset, int len);
void objectPrintf(ObjectPtr object, int offset, const char *format, ...)
     ATTRIBUTE ((format (printf, 3, 4)));
int discardObjectsHandler(TimeEventHandlerPtr);
void writeoutObjects(int);
int discardObjects(int all, int force);
int objectIsStale(ObjectPtr object, CacheControlPtr cache_control)
    ATTRIBUTE ((pure));
int objectMustRevalidate(ObjectPtr object, CacheControlPtr cache_control)
    ATTRIBUTE ((pure));
