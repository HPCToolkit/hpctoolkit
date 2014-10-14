#ifndef SS_OBJ_NAME_H
#define SS_OBJ_NAME_H

#define SSON_cat3(a, b, c) a ## b ## c
#define SS_OBJ_NAME(name) SSON_cat3(_,name,_obj)

#endif // SS_OBJ_NAME_H
