#ifndef JS_DEFINED_H_
#define JS_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(PICORB_VM_MRUBYC)
struct VM;
void mrbc_js_init(struct VM *vm);
#else
struct mrb_state;
void mrb_js_init(struct mrb_state *mrb);
#endif

#ifdef __cplusplus
}
#endif

#endif /* JS_DEFINED_H_ */


