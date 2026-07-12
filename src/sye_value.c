#include "../include/sye_value.h"
#include <stdlib.h>
#include <string.h>

static char *vdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *out = (char*)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

SyeValue sye_value_null(void) {
    SyeValue v;
    memset(&v, 0, sizeof(v));
    v.kind = SYE_VALUE_NULL;
    return v;
}

SyeValue sye_value_bool(int value) {
    SyeValue v = sye_value_null();
    v.kind = SYE_VALUE_BOOL;
    v.as.boolean = value ? 1 : 0;
    return v;
}

SyeValue sye_value_number(double value) {
    SyeValue v = sye_value_null();
    v.kind = SYE_VALUE_NUMBER;
    v.as.number = value;
    return v;
}

SyeValue sye_value_string(const char *value) {
    SyeValue v = sye_value_null();
    v.kind = SYE_VALUE_STRING;
    v.as.string = vdup(value);
    return v;
}

SyeValue sye_value_array(void) {
    SyeValue v = sye_value_null();
    v.kind = SYE_VALUE_ARRAY;
    return v;
}

SyeValue sye_value_object(void) {
    SyeValue v = sye_value_null();
    v.kind = SYE_VALUE_OBJECT;
    return v;
}

static int array_grow(SyeArray *array) {
    if (array->count < array->capacity) return 1;
    int next = array->capacity ? array->capacity * 2 : 8;
    SyeValue *items = (SyeValue*)realloc(array->items, (size_t)next * sizeof(SyeValue));
    if (!items) return 0;
    array->items = items;
    array->capacity = next;
    return 1;
}

static int object_grow(SyeObject *object) {
    if (object->count < object->capacity) return 1;
    int next = object->capacity ? object->capacity * 2 : 8;
    SyeObjectEntry *items = (SyeObjectEntry*)realloc(object->items, (size_t)next * sizeof(SyeObjectEntry));
    if (!items) return 0;
    object->items = items;
    object->capacity = next;
    return 1;
}

void sye_value_free(SyeValue *value) {
    if (!value) return;
    if (value->kind == SYE_VALUE_STRING) {
        free(value->as.string);
    } else if (value->kind == SYE_VALUE_ARRAY) {
        for (int i = 0; i < value->as.array.count; ++i) sye_value_free(&value->as.array.items[i]);
        free(value->as.array.items);
    } else if (value->kind == SYE_VALUE_OBJECT) {
        for (int i = 0; i < value->as.object.count; ++i) {
            free(value->as.object.items[i].key);
            sye_value_free(&value->as.object.items[i].value);
        }
        free(value->as.object.items);
    }
    *value = sye_value_null();
}

SyeValue sye_value_clone(const SyeValue *value) {
    if (!value) return sye_value_null();
    switch (value->kind) {
        case SYE_VALUE_BOOL: return sye_value_bool(value->as.boolean);
        case SYE_VALUE_NUMBER: return sye_value_number(value->as.number);
        case SYE_VALUE_STRING: return sye_value_string(value->as.string);
        case SYE_VALUE_ARRAY: {
            SyeValue out = sye_value_array();
            for (int i = 0; i < value->as.array.count; ++i)
                sye_array_push(&out, sye_value_clone(&value->as.array.items[i]));
            return out;
        }
        case SYE_VALUE_OBJECT: {
            SyeValue out = sye_value_object();
            for (int i = 0; i < value->as.object.count; ++i)
                sye_object_put(&out, value->as.object.items[i].key, sye_value_clone(&value->as.object.items[i].value));
            return out;
        }
        case SYE_VALUE_FUNCTION:
            return *value;
        case SYE_VALUE_NULL:
        default:
            return sye_value_null();
    }
}

const char *sye_value_kind_name(SyeValueKind kind) {
    switch (kind) {
        case SYE_VALUE_NULL: return "null";
        case SYE_VALUE_BOOL: return "bool";
        case SYE_VALUE_NUMBER: return "number";
        case SYE_VALUE_STRING: return "string";
        case SYE_VALUE_ARRAY: return "array";
        case SYE_VALUE_OBJECT: return "object";
        case SYE_VALUE_FUNCTION: return "function";
        default: return "unknown";
    }
}

int sye_array_push(SyeValue *array_value, SyeValue item) {
    if (!array_value || array_value->kind != SYE_VALUE_ARRAY) {
        sye_value_free(&item);
        return 0;
    }
    if (!array_grow(&array_value->as.array)) {
        sye_value_free(&item);
        return 0;
    }
    array_value->as.array.items[array_value->as.array.count++] = item;
    return 1;
}

int sye_object_put(SyeValue *object_value, const char *key, SyeValue value) {
    if (!object_value || object_value->kind != SYE_VALUE_OBJECT || !key) {
        sye_value_free(&value);
        return 0;
    }
    for (int i = 0; i < object_value->as.object.count; ++i) {
        if (strcmp(object_value->as.object.items[i].key, key) == 0) {
            sye_value_free(&object_value->as.object.items[i].value);
            object_value->as.object.items[i].value = value;
            return 1;
        }
    }
    if (!object_grow(&object_value->as.object)) {
        sye_value_free(&value);
        return 0;
    }
    int i = object_value->as.object.count++;
    object_value->as.object.items[i].key = vdup(key);
    object_value->as.object.items[i].value = value;
    if (!object_value->as.object.items[i].key) {
        sye_value_free(&object_value->as.object.items[i].value);
        object_value->as.object.count--;
        return 0;
    }
    return 1;
}

SyeValue *sye_object_get(SyeValue *object_value, const char *key) {
    if (!object_value || object_value->kind != SYE_VALUE_OBJECT || !key) return NULL;
    for (int i = 0; i < object_value->as.object.count; ++i)
        if (strcmp(object_value->as.object.items[i].key, key) == 0) return &object_value->as.object.items[i].value;
    return NULL;
}
