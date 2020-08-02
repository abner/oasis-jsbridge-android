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
#ifndef _JSBRIDGE_JAVASCRIPTMETHOD_H
#define _JSBRIDGE_JAVASCRIPTMETHOD_H

#include "JniTypes.h"
#include "jni-helpers/JniGlobalRef.h"
#include <string>
#include <vector>

#ifdef QUICKJS
# include "quickjs/quickjs.h"
#endif

class JavaType;
class JsBridgeContext;
class JObjectArrayLocalRef;
class JValue;

class JavaScriptMethod {
public:
  JavaScriptMethod(const JsBridgeContext *, const JniRef<jsBridgeMethod> &method, std::string methodName, bool isLambda);

  JavaScriptMethod(const JavaScriptMethod &) = delete;
  JavaScriptMethod& operator=(const JavaScriptMethod &) = delete;

  JavaScriptMethod(JavaScriptMethod &&) noexcept;
  JavaScriptMethod &operator=(JavaScriptMethod &&) noexcept ;

  const std::string &getName() const { return m_methodName; }

#if defined(DUKTAPE)
  JValue invoke(const JsBridgeContext *, void *jsHeapPtr, const JObjectArrayLocalRef &args, bool awaitJsPromise) const;
#elif defined(QUICKJS)
  JValue invoke(const JsBridgeContext *, JSValueConst jsMethod, JSValueConst jsThis, const JObjectArrayLocalRef &args, bool awaitJsPromise) const;
#endif

private:
  std::string m_methodName;
  std::unique_ptr<const JavaType> m_returnValueType;
  JniGlobalRef<jsBridgeParameter> m_returnValueParameter;
  std::vector<std::unique_ptr<const JavaType>> m_argumentTypes;
  bool m_isLambda;
  bool m_isVarArgs;
};

#endif
