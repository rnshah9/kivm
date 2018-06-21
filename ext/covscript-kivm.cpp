/*
 * Covariant Script KiVM Extension
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2018 Kiva
 * Email: libkernelpanic@gmail.com
 * Github: https://github.com/imkiva
 */

#include "extension-helper.h"
#include <kivm/kivm.h>
#include <kivm/oop/method.h>
#include <kivm/oop/klass.h>
#include <kivm/oop/instanceKlass.h>
#include <kivm/classpath/classLoader.h>
#include <kivm/runtime/nativeMethodPool.h>
#include <kivm/runtime/javaThread.h>
#include <kivm/bytecode/invocationContext.h>
#include <kivm/runtime/javaThread.h>
#include <kivm/oop/instanceOop.h>
#include <kivm/oop/primitiveOop.h>
#include <kivm/oop/mirrorOop.h>
#include <kivm/oop/arrayOop.h>

#include <cstdarg>
#include <sparsepp/spp.h>

CS_EXTENSION(kivm) // NOLINT

template <typename... ArgsT>
cs::var invoke(const cs::var &func, cs::vector &args)
{
	if (func.type() == typeid(cs::callable)) {
		return func.const_val<cs::callable>().call(args);
	}
	else if (func.type() == typeid(cs::object_method)) {
		const auto &om = func.const_val<cs::object_method>();
		return om.callable.const_val<cs::callable>().call(args);
	}
	return cs::null_pointer;
}

class CSKiVMInstance {
public:
	static CSKiVMInstance *get()
	{
		static CSKiVMInstance instance;
		return &instance;
	}

	static kivm::Method *currentMethod()
	{
		auto thread = kivm::Threads::currentThread();
		if (thread == nullptr) {
			throw cs::lang_error("cs-jni methods called without JNI environment");
		}
		return thread->getCurrentMethod();
	}

	static void csNativeCallerVoid(JNIEnv *env, ...)
	{
		using namespace kivm;

		auto instance = get();
		auto method = currentMethod();
		auto iter = instance->_csNatives.find(method);
		if (iter == instance->_csNatives.end()) {
			return;
		}

		va_list ap;
		va_start(ap, env);

		cs::vector args;
		if (method->isStatic()) {
			// jclass
			(void)va_arg(ap, jobject);
		}
		else {
			// Java this object
			args.push_back(va_arg(ap, jobject));
		}

		const auto &descriptorMap = method->getArgumentValueTypes();
		for (auto it = descriptorMap.begin(); it != descriptorMap.end(); ++it) {
			ValueType valueType = *it;

			// fill values
			switch (valueType) {
			case ValueType::INT: {
				args.push_back(va_arg(ap, jint));
				break;
			}
			case ValueType::LONG: {
				args.push_back(va_arg(ap, jlong));
				break;
			}
			case ValueType::FLOAT:
			case ValueType::DOUBLE: {
				args.push_back(va_arg(ap, jdouble));
				break;
			}
			case ValueType::OBJECT:
			case ValueType::ARRAY: {
				args.push_back(va_arg(ap, jobject));
				break;
			}
			default:
				throw cs::lang_error("cs-jni methods called without JNI environment");
			}
		}
		va_end(ap);
		invoke(iter->second, args);
	}

	static jint csNativeCallerInt(JNIEnv *env, ...)
	{
		return 0;
	}

	static jlong csNativeCallerLong(JNIEnv *env, ...)
	{
		return 0;
	}

	static jfloat csNativeCallerFloat(JNIEnv *env, ...)
	{
		return 0;
	}

	static jdouble csNativeCallerDouble(JNIEnv *env, ...)
	{
		return 0;
	}

	static jobject csNativeCallerObject(JNIEnv *env, ...)
	{
		return 0;
	}

	spp::sparse_hash_map<kivm::Method *, cs::var> _csNatives;
};

CNI_NORMAL(void, registerNative,
           const cs::string &className,
           const cs::string &name,
           const cs::string &descriptor,
           const cs::var &func)
{
	using namespace kivm;

	auto cl = kivm::BootstrapClassLoader::get();
	auto javaClass = cl->loadClass(strings::replaceAll(strings::fromStdString(className),
	                               Global::DOT, Global::SLASH));

	if (javaClass == nullptr) {
		throw cs::lang_error("java.lang.ClassNotFoundException: " + className);
	}

	if (javaClass->getClassType() != ClassType::INSTANCE_CLASS) {
		throw cs::lang_error("java.lang.IllegalArgumentException: class " + className + " is not an instance class");
	}

	auto instanceClass = (InstanceKlass *)javaClass;
	auto method = instanceClass->getThisClassMethod(strings::fromStdString(name),
	              strings::fromStdString(descriptor));
	if (method == nullptr) {
		throw cs::lang_error("java.lang.NoSuchMethodError: " + name + ":" + descriptor);
	}

	CSKiVMInstance::get()->_csNatives[method] = func;

	if (!method->isNative()) {
		method->hackAsNative();
	}

	switch (method->getReturnType()) {
	case ValueType::INT:
		NativeMethodPool::get()->set(method, (void *)&CSKiVMInstance::csNativeCallerInt);
		break;
	case ValueType::LONG:
		NativeMethodPool::get()->set(method, (void *)&CSKiVMInstance::csNativeCallerLong);
		break;
	case ValueType::FLOAT:
		NativeMethodPool::get()->set(method, (void *)&CSKiVMInstance::csNativeCallerFloat);
		break;
	case ValueType::DOUBLE:
		NativeMethodPool::get()->set(method, (void *)&CSKiVMInstance::csNativeCallerDouble);
		break;
	case ValueType::OBJECT:
	case ValueType::ARRAY:
		NativeMethodPool::get()->set(method, (void *)&CSKiVMInstance::csNativeCallerObject);
		break;
	case ValueType::VOID:
		NativeMethodPool::get()->set(method, (void *)&CSKiVMInstance::csNativeCallerVoid);
		break;
	default:
		throw cs::internal_error("should not reach here");
	}
}

CNI_NORMAL(void, javaInit)
{
	using namespace kivm;

	JavaVM *javaVM = nullptr;
	JNIEnv *env = nullptr;
	if (JNI_CreateJavaVM(&javaVM, (void **)&env, nullptr) != JNI_OK) {
		throw cs::internal_error("JNI_CreateJavaVM() failed");
	}
}

CNI_NORMAL(void, javaDestroy)
{
	using namespace kivm;
	JavaVM *javaVM = KiVM::getJavaVMQuick();
	if (javaVM != nullptr) {
		javaVM->DestroyJavaVM();
	}
}

CNI_NORMAL(void, javaMain,
           const cs::string &className,
           const cs::array &args)
{
	using namespace kivm;

	String mainClassName = strings::replaceAll(strings::fromStdString(className),
	                       Global::DOT, Global::SLASH);
	std::vector<String> arguments;
	for (auto &e : args) {
		arguments.push_back(strings::fromStdString(e.const_val<cs::string>()));
	}

	JavaMainThread javaMainThread(mainClassName, arguments);
	javaMainThread.start();
}

class javaMethod final {
	kivm::MethodID *_methodId = nullptr;

public:
	javaMethod() = delete;
	javaMethod(const javaMethod &) = default;
	explicit javaMethod(kivm::MethodID *ptr) : _methodId(ptr) {}
	cs::var operator()(cs::vector &args)
	{
		using namespace kivm;
		auto thread = Threads::currentThread();
		if (thread == nullptr)
			throw cs::lang_error("java.lang.IllegalStateException: not a Java thread");
		const auto &dest_args = _methodId->_method->getArgumentValueTypes();
		if (args.size() != dest_args.size())
			throw cs::lang_error("Wrong size of arguments.");
		std::list<oop> jargs;
		for (std::size_t i = 0; i < args.size(); ++i) {
			cs::var &arg = args[i];
			if (arg.type() == typeid(cs::number)) {
				switch (dest_args[i]) {
				case ValueType::INT: {
					jargs.push_back(new intOopDesc(arg.const_val<cs::number>()));
					break;
				}
				case ValueType::LONG: {
					args.push_back(new longOopDesc(arg.const_val<cs::number>()));
					break;
				}
				case ValueType::FLOAT: {
					args.push_back(new floatOopDesc(arg.const_val<cs::number>()));
					break;
				}
				case ValueType::DOUBLE: {
					args.push_back(new doubleOopDesc(arg.const_val<cs::number>()));
					break;
				}
				default:
					throw cs::lang_error("Unsupported type conversion form CovScript to Java.");
				}
			}
			else if (arg.type() == typeid(cs::string)) {
				// TODO
			}
			else
				throw cs::lang_error("Unsupported type conversion form CovScript to Java.");
		}
		oop rv = InvocationContext::invokeWithArgs(thread, _methodId->_method, jargs);
		if (thread->isExceptionOccurred())
			KiVM::uncaughtException(thread);
		if (rv == nullptr)
			return cs::null_pointer;
		switch (_methodId->_method->getReturnType()) {
		default:
			return cs::null_pointer;
		}
	}
};

CNI_NORMAL(cs::type, findClass, const cs::string &className)
{
	using namespace kivm;
	auto javaClass = kivm::BootstrapClassLoader::get()->loadClass(strings::replaceAll(strings::fromStdString(className), Global::DOT, Global::SLASH));
	if (javaClass == nullptr)
		throw cs::lang_error("java.lang.ClassNotFoundException: " + className);
	if (javaClass->getClassType() != ClassType::INSTANCE_CLASS)
		throw cs::lang_error("java.lang.IllegalArgumentException: class " + className + " is not an instance class");
	auto instanceClass = (InstanceKlass *)javaClass;
	cs::name_space ns;
	for (auto &e : instanceClass->getDeclaredMethods())
		if (e.second->_method->isStatic())
			ns.add_var(strings::toStdString(e.second->_method->getName()), cs::callable(javaMethod(e.second)));
	return cs::type([]() -> cs::var { return 0; }, cs_impl::hash(className), std::make_shared<cs::extension_holder>(ns.get_domain()));
}

CS_EXTENSION_END()
