#ifndef JS_DEFINED_H_
#define JS_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(PICORUBY_VM_MRUBYC)
void mrbc_js_init(mrbc_vm *vm);
#else
// TODO for mruby (MicroRuby)
#endif

#ifdef __cplusplus
}
#endif

#endif /* JS_DEFINED_H_ */


