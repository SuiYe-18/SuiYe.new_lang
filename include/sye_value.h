#ifndef SYE_VALUE_H
#define SYE_VALUE_H

typedef enum {
    SYE_VALUE_NULL = 0,
    SYE_VALUE_BOOL,
    SYE_VALUE_NUMBER,
    SYE_VALUE_STRING,
    SYE_VALUE_ARRAY,
    SYE_VALUE_OBJECT,
    SYE_VALUE_FUNCTION
} SyeValueKind;

typedef struct SyeValue SyeValue;
typedef struct SyeArray SyeArray;
typedef struct SyeObject SyeObject;
typedef struct SyeObjectEntry SyeObjectEntry;
typedef struct SyeClosure SyeClosure;

struct SyeArray {
    SyeValue *items;
    int count;
    int capacity;
};

struct SyeObject {
    SyeObjectEntry *items;
    int count;
    int capacity;
};

struct SyeValue {
    SyeValueKind kind;
    union {
        int boolean;
        double number;
        char *string;
        SyeArray array;
        SyeObject object;
        SyeClosure *closure;
    } as;
};

struct SyeObjectEntry {
    char *key;
    SyeValue value;
};

SyeValue sye_value_null(void);
SyeValue sye_value_bool(int value);
SyeValue sye_value_number(double value);
SyeValue sye_value_string(const char *value);
SyeValue sye_value_array(void);
SyeValue sye_value_object(void);
void sye_value_free(SyeValue *value);
SyeValue sye_value_clone(const SyeValue *value);
const char *sye_value_kind_name(SyeValueKind kind);

int sye_array_push(SyeValue *array_value, SyeValue item);
int sye_object_put(SyeValue *object_value, const char *key, SyeValue value);
SyeValue *sye_object_get(SyeValue *object_value, const char *key);

#endif
