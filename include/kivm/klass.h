//
// Created by kiva on 2018/2/25.
//
#pragma once

#include <unordered_map>
#include <kivm/kivm.h>
#include <kivm/classFile.h>
#include <kivm/classLoader.h>

namespace kivm {
    enum ClassType {
        INSTANCE_CLASS,
        OBJECT_ARRAY_CLASS,
        TYPE_ARRAY_CLASS,
    };

    enum oopType {
        INSTANCE_OOP,
        PRIMITIVE_OOP,
        OBJECT_ARRAY_OOP,
        TYPE_ARRAY_OOP,
    };

    enum ClassState {
        ALLOCATED,
        LOADED,
        LINKED,
        BEING_INITIALIZED,
        FULLY_INITIALIZED,
        INITIALIZATION_ERROR,
    };

    class Klass {
    private:
        ClassState _state;
        u2 _access_flag;

    protected:
        String _name;
        ClassType _type;

        Klass *_super_class;

    public:
        ClassState get_state() const {
            return _state;
        }

        void set_state(ClassState _state) {
            this->_state = _state;
        }

        u2 get_access_flag() const {
            return _access_flag;
        }

        void set_access_flag(u2 _access_flag) {
            Klass::_access_flag = _access_flag;
        }

        const String &get_name() const {
            return _name;
        }

        void set_name(const String &_name) {
            this->_name = _name;
        }

        ClassType get_type() const {
            return _type;
        }

        void set_type(ClassType _type) {
            this->_type = _type;
        }

        Klass *get_super_class() const {
            return _super_class;
        }

        void set_super_class(Klass *_super_class) {
            this->_super_class = _super_class;
        }

        bool is_public() const {
            return (get_access_flag() & ACC_PUBLIC) == ACC_PUBLIC;
        }

        bool is_private() const {
            return (get_access_flag() & ACC_PRIVATE) == ACC_PRIVATE;
        }

        bool is_protected() const {
            return (get_access_flag() & ACC_PROTECTED) == ACC_PROTECTED;
        }

        bool is_final() const {
            return (get_access_flag() & ACC_FINAL) == ACC_FINAL;
        }

        bool is_static() {
            return (get_access_flag() & ACC_STATIC) == ACC_STATIC;
        }

        bool is_abstract() {
            return (get_access_flag() & ACC_ABSTRACT) == ACC_ABSTRACT;
        }

    public:
        Klass();

        virtual ~Klass() = default;

        virtual void link_and_init() = 0;
    };
}

