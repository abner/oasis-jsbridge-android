/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Originally based on Duktape Android:
 * Copyright (C) 2015 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "Integer.h"

#include "JsBridgeContext.h"
#include "jni-helpers/JArrayLocalRef.h"

#if defined(DUKTAPE)
# include "JsBridgeContext.h"
# include "StackChecker.h"
# include "StackUnwinder.h"
#endif

namespace JavaTypes {

Integer::Integer(const JsBridgeContext *jsBridgeContext)
 : Primitive(jsBridgeContext, JavaTypeId::Int, JavaTypeId::BoxedInt) {
}

#if defined(DUKTAPE)

JValue Integer::pop(bool inScript) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to int";
    duk_pop(m_ctx);
    CHECK_STACK_NOW();
    m_jsBridgeContext->throwTypeException(message, inScript);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  int i = duk_require_int(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(i);
}

JValue Integer::popArray(uint32_t count, bool expanded, bool inScript) const {
  // If we're not expanded, pop the array off the stack no matter what.
  const StackUnwinder _(m_ctx, expanded ? 0 : 1);

  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Integer>";
      m_jsBridgeContext->throwTypeException(message, inScript);
    }
  }

  JArrayLocalRef<jint> intArray(m_jniContext, count);
  m_jsBridgeContext->checkRethrowJsError();

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, i);
    }
    JValue value = pop(inScript);
    intArray.setElement(i, value.getInt());
  }

  return JValue(intArray);
}

duk_ret_t Integer::push(const JValue &value, bool inScript) const {
  duk_push_int(m_ctx, value.getInt());
  return 1;
}

duk_ret_t Integer::pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript) const {
  JArrayLocalRef<jint> intArray(values);
  const auto count = intArray.getLength();

  if (!expand) {
    duk_push_array(m_ctx);
  }
  for (jsize i = 0; i < count; ++i) {
    jint element = intArray.getElement(i);
    duk_push_int(m_ctx, element);
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }
  return expand ? count : 1;
}

#elif defined(QUICKJS)

JValue Integer::toJava(JSValueConst v, bool inScript) const {
  if (!JS_IsNumber(v)) {
    const char *message = "Cannot convert return value to int";
    m_jsBridgeContext->throwTypeException(message, inScript);
    return JValue();
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  int i;
  if (JS_IsInteger(v)) {
    i = JS_VALUE_GET_INT(v);
  } else {
    i = int(JS_VALUE_GET_FLOAT64(v));
  }

  return JValue(i);
}

JValue Integer::toJavaArray(JSValueConst v, bool inScript) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  if (!JS_IsArray(m_ctx, v)) {
    m_jsBridgeContext->throwTypeException("Cannot convert JS value to Java array", inScript);
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JArrayLocalRef<jint> intArray(m_jniContext, count);

  for (uint32_t i = 0; i < count; ++i) {
    JValue elementValue = toJava(JS_GetPropertyUint32(m_ctx, v, i), inScript);
    intArray.setElement(i, elementValue.getInt());
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(intArray);
}

JSValue Integer::fromJava(const JValue &value, bool /*inScript*/) const {
  return JS_NewInt32(m_ctx, value.getInt());
}

JSValue Integer::fromJavaArray(const JniLocalRef<jarray> &values, bool inScript) const {
  JArrayLocalRef<jint> intArray(values);
  const auto count = intArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  // TODO: use std::vector<jint> objectArray.getAllElements() when available
  for (jsize i = 0; i < count; ++i) {
    jint iValue = intArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(iValue), inScript);
      JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
    } catch (std::invalid_argument &e) {
      JS_FreeValue(m_ctx, jsArray);
      throw e;
    }
  }

  return jsArray;
}

#endif

JValue Integer::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                const std::vector<JValue> &args) const {
  jint returnValue = m_jniContext->callIntMethodA(javaThis, methodId, args);
  m_jsBridgeContext->checkRethrowJsError();

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue(returnValue);
}

JValue Integer::box(const JValue &intValue) const {
  // From int to Integer
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(I)Ljava/lang/Integer;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, intValue.getInt()));
}

JValue Integer::unbox(const JValue &boxedValue) const {
  // From Integer to int
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "intValue", "()I");
  return JValue(m_jniContext->callIntMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

