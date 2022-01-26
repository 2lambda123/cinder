# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import _testcapi
import asyncio
import builtins
import dis
import gc
import sys
import tempfile
import threading
import types
import unittest
import warnings
import weakref
from compiler.consts import CO_SUPPRESS_JIT, CO_NORMAL_FRAME
from compiler.static import StaticCodeGenerator
from contextlib import contextmanager
from functools import cmp_to_key
from pathlib import Path
from textwrap import dedent

try:
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        from .test_compiler.test_static.common import StaticTestBase
except ImportError:
    from test_compiler.test_static.common import StaticTestBase

from contextlib import contextmanager

try:
    import cinderjit
    from cinderjit import jit_suppress
except:
    cinderjit = None

    def jit_suppress(func):
        return func


class GetFrameLineNumberTests(unittest.TestCase):
    def assert_code_and_lineno(self, frame, func, lineno):
        self.assertEqual(frame.f_code, func.__code__)
        self.assertEqual(frame.f_lineno, lineno)

    def test_line_numbers(self):
        """Verify that line numbers are correct"""

        @unittest.failUnlessJITCompiled
        def g():
            return sys._getframe()

        self.assert_code_and_lineno(g(), g, 50)

    def test_line_numbers_for_running_generators(self):
        """Verify that line numbers are correct for running generator functions"""

        @unittest.failUnlessJITCompiled
        def g(x, y):
            yield sys._getframe()
            z = x + y
            yield sys._getframe()
            yield z

        initial_lineno = 59
        gen = g(1, 2)
        frame = next(gen)
        self.assert_code_and_lineno(frame, g, initial_lineno)
        frame = next(gen)
        self.assert_code_and_lineno(frame, g, initial_lineno + 2)
        self.assertEqual(next(gen), 3)

    def test_line_numbers_for_suspended_generators(self):
        """Verify that line numbers are correct for suspended generator functions"""

        @unittest.failUnlessJITCompiled
        def g(x):
            x = x + 1
            yield x
            z = x + 1
            yield z

        gen = g(0)
        initial_lineno = 75
        self.assert_code_and_lineno(gen.gi_frame, g, initial_lineno)
        v = next(gen)
        self.assertEqual(v, 1)
        self.assert_code_and_lineno(gen.gi_frame, g, initial_lineno + 3)
        v = next(gen)
        self.assertEqual(v, 2)
        self.assert_code_and_lineno(gen.gi_frame, g, initial_lineno + 5)

    def test_line_numbers_during_gen_throw(self):
        """Verify that line numbers are correct for suspended generator functions when
        an exception is thrown into them.
        """

        @unittest.failUnlessJITCompiled
        def f1(g):
            yield from g

        @unittest.failUnlessJITCompiled
        def f2(g):
            yield from g

        gen1, gen2 = None, None
        gen1_frame, gen2_frame = None, None

        @unittest.failUnlessJITCompiled
        def f3():
            nonlocal gen1_frame, gen2_frame
            try:
                yield "hello"
            except TestException:
                gen1_frame = gen1.gi_frame
                gen2_frame = gen2.gi_frame
                raise

        gen3 = f3()
        gen2 = f2(gen3)
        gen1 = f1(gen2)
        gen1.send(None)
        with self.assertRaises(TestException):
            gen1.throw(TestException())
        initial_lineno = 99
        self.assert_code_and_lineno(gen1_frame, f1, initial_lineno)
        self.assert_code_and_lineno(gen2_frame, f2, initial_lineno + 4)


# Decorator to return a new version of the function with an alternate globals
# dict.
def with_globals(gbls):
    def decorator(func):
        new_func = type(func)(
            func.__code__, gbls, func.__name__, func.__defaults__, func.__closure__
        )
        new_func.__module__ = func.__module__
        new_func.__kwdefaults__ = func.__kwdefaults__
        return new_func

    return decorator


@unittest.failUnlessJITCompiled
def get_meaning_of_life(obj):
    return obj.meaning_of_life()


def nothing():
    return 0


def _simpleFunc(a, b):
    return a, b


class _CallableObj:
    def __call__(self, a, b):
        return self, a, b


class CallKWArgsTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def test_call_basic_function_pos_and_kw(self):
        r = _simpleFunc(1, b=2)
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_basic_function_kw_only(self):
        r = _simpleFunc(b=2, a=1)
        self.assertEqual(r, (1, 2))

        r = _simpleFunc(a=1, b=2)
        self.assertEqual(r, (1, 2))

    @staticmethod
    def _f1(a, b):
        return a, b

    @unittest.failUnlessJITCompiled
    def test_call_class_static_pos_and_kw(self):
        r = CallKWArgsTests._f1(1, b=2)
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_class_static_kw_only(self):
        r = CallKWArgsTests._f1(b=2, a=1)
        self.assertEqual(r, (1, 2))

    def _f2(self, a, b):
        return self, a, b

    @unittest.failUnlessJITCompiled
    def test_call_method_kw_and_pos(self):
        r = self._f2(1, b=2)
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_method_kw_only(self):
        r = self._f2(b=2, a=1)
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_bound_method_kw_and_pos(self):
        f = self._f2
        r = f(1, b=2)
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_bound_method_kw_only(self):
        f = self._f2
        r = f(b=2, a=1)
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_obj_kw_and_pos(self):
        o = _CallableObj()
        r = o(1, b=2)
        self.assertEqual(r, (o, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_obj_kw_only(self):
        o = _CallableObj()
        r = o(b=2, a=1)
        self.assertEqual(r, (o, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_c_func(self):
        self.assertEqual(__import__("sys", globals=None), sys)


class CallExTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def test_call_dynamic_kw_dict(self):
        r = _simpleFunc(**{"b": 2, "a": 1})
        self.assertEqual(r, (1, 2))

    class _DummyMapping:
        def keys(self):
            return ("a", "b")

        def __getitem__(self, k):
            return {"a": 1, "b": 2}[k]

    @unittest.failUnlessJITCompiled
    def test_call_dynamic_kw_dict(self):
        r = _simpleFunc(**CallExTests._DummyMapping())
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_dynamic_pos_tuple(self):
        r = _simpleFunc(*(1, 2))
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_dynamic_pos_list(self):
        r = _simpleFunc(*[1, 2])
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_dynamic_pos_and_kw(self):
        r = _simpleFunc(*(1,), **{"b": 2})
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def _doCall(self, args, kwargs):
        return _simpleFunc(*args, **kwargs)

    def test_invalid_kw_type(self):
        err = r"_simpleFunc\(\) argument after \*\* must be a mapping, not int"
        with self.assertRaisesRegex(TypeError, err):
            self._doCall([], 1)

    @unittest.skipUnlessCinderJITEnabled("Exposes interpreter reference leak")
    def test_invalid_pos_type(self):
        err = r"_simpleFunc\(\) argument after \* must be an iterable, not int"
        with self.assertRaisesRegex(TypeError, err):
            self._doCall(1, {})

    @staticmethod
    def _f1(a, b):
        return a, b

    @unittest.failUnlessJITCompiled
    def test_call_class_static_pos_and_kw(self):
        r = CallExTests._f1(*(1,), **{"b": 2})
        self.assertEqual(r, (1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_class_static_kw_only(self):
        r = CallKWArgsTests._f1(**{"b": 2, "a": 1})
        self.assertEqual(r, (1, 2))

    def _f2(self, a, b):
        return self, a, b

    @unittest.failUnlessJITCompiled
    def test_call_method_kw_and_pos(self):
        r = self._f2(*(1,), **{"b": 2})
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_method_kw_only(self):
        r = self._f2(**{"b": 2, "a": 1})
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_bound_method_kw_and_pos(self):
        f = self._f2
        r = f(*(1,), **{"b": 2})
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_bound_method_kw_only(self):
        f = self._f2
        r = f(**{"b": 2, "a": 1})
        self.assertEqual(r, (self, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_obj_kw_and_pos(self):
        o = _CallableObj()
        r = o(*(1,), **{"b": 2})
        self.assertEqual(r, (o, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_obj_kw_only(self):
        o = _CallableObj()
        r = o(**{"b": 2, "a": 1})
        self.assertEqual(r, (o, 1, 2))

    @unittest.failUnlessJITCompiled
    def test_call_c_func_pos_only(self):
        self.assertEqual(len(*([2],)), 1)

    @unittest.failUnlessJITCompiled
    def test_call_c_func_pos_and_kw(self):
        self.assertEqual(__import__(*("sys",), **{"globals": None}), sys)


class LoadMethodCacheTests(unittest.TestCase):
    def test_type_modified(self):
        class Oracle:
            def meaning_of_life(self):
                return 42

        obj = Oracle()
        # Uncached
        self.assertEqual(get_meaning_of_life(obj), 42)
        # Cached
        self.assertEqual(get_meaning_of_life(obj), 42)

        # Invalidate cache
        def new_meaning_of_life(x):
            return 0

        Oracle.meaning_of_life = new_meaning_of_life

        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_base_type_modified(self):
        class Base:
            def meaning_of_life(self):
                return 42

        class Derived(Base):
            pass

        obj = Derived()
        # Uncached
        self.assertEqual(get_meaning_of_life(obj), 42)
        # Cached
        self.assertEqual(get_meaning_of_life(obj), 42)

        # Mutate Base. Should propagate to Derived and invalidate the cache.
        def new_meaning_of_life(x):
            return 0

        Base.meaning_of_life = new_meaning_of_life

        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_second_base_type_modified(self):
        class Base1:
            pass

        class Base2:
            def meaning_of_life(self):
                return 42

        class Derived(Base1, Base2):
            pass

        obj = Derived()
        # Uncached
        self.assertEqual(get_meaning_of_life(obj), 42)
        # Cached
        self.assertEqual(get_meaning_of_life(obj), 42)

        # Mutate first base. Should propagate to Derived and invalidate the cache.
        def new_meaning_of_life(x):
            return 0

        Base1.meaning_of_life = new_meaning_of_life

        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_type_dunder_bases_reassigned(self):
        class Base1:
            pass

        class Derived(Base1):
            pass

        # No shadowing happens between obj{1,2} and Derived, thus the now
        # shadowing flag should be set
        obj1 = Derived()
        obj2 = Derived()
        obj2.meaning_of_life = nothing

        # Now obj2.meaning_of_life shadows Base.meaning_of_life
        class Base2:
            def meaning_of_life(self):
                return 42

        Derived.__bases__ = (Base2,)

        # Attempt to prime the cache
        self.assertEqual(get_meaning_of_life(obj1), 42)
        self.assertEqual(get_meaning_of_life(obj1), 42)
        # If flag is not correctly cleared when Derived.__bases__ is
        # assigned we will end up returning 42
        self.assertEqual(get_meaning_of_life(obj2), 0)

    def _make_obj(self):
        class Oracle:
            def meaning_of_life(self):
                return 42

        obj = Oracle()
        # Uncached
        self.assertEqual(get_meaning_of_life(obj), 42)
        # Cached
        self.assertEqual(get_meaning_of_life(obj), 42)
        return obj

    def test_instance_assignment(self):
        obj = self._make_obj()
        obj.meaning_of_life = nothing
        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_instance_dict_assignment(self):
        obj = self._make_obj()
        obj.__dict__["meaning_of_life"] = nothing
        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_instance_dict_replacement(self):
        obj = self._make_obj()
        obj.__dict__ = {"meaning_of_life": nothing}
        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_instance_dunder_class_assignment(self):
        obj = self._make_obj()

        class Other:
            pass

        other = Other()
        other.meaning_of_life = nothing
        other.__class__ = obj.__class__
        self.assertEqual(get_meaning_of_life(other), 0)

    def test_shadowcode_setattr(self):
        """sets attribute via shadow byte code, it should update the
        type bit for instance shadowing"""
        obj = self._make_obj()
        obj.foo = 42
        obj1 = type(obj)()
        obj1.other = 100

        def f(obj, set):
            if set:
                obj.meaning_of_life = nothing
            yield 42

        for i in range(100):
            list(f(obj, False))
        list(f(obj, True))

        self.assertEqual(get_meaning_of_life(obj), 0)

    def test_shadowcode_setattr_split(self):
        """sets attribute via shadow byte code on a split dict,
        it should update the type bit for instance shadowing"""
        obj = self._make_obj()

        def f(obj, set):
            if set:
                obj.meaning_of_life = nothing
            yield 42

        for i in range(100):
            list(f(obj, False))
        list(f(obj, True))

        self.assertEqual(get_meaning_of_life(obj), 0)


@unittest.failUnlessJITCompiled
def get_foo(obj):
    return obj.foo


class LoadAttrCacheTests(unittest.TestCase):
    def test_dict_reassigned(self):
        class Base:
            def __init__(self, x):
                self.foo = x

        obj1 = Base(100)
        obj2 = Base(200)
        # uncached
        self.assertEqual(get_foo(obj1), 100)
        # cached
        self.assertEqual(get_foo(obj1), 100)
        self.assertEqual(get_foo(obj2), 200)
        obj1.__dict__ = {"foo": 200}
        self.assertEqual(get_foo(obj1), 200)
        self.assertEqual(get_foo(obj2), 200)

    def test_dict_mutated(self):
        class Base:
            def __init__(self, foo):
                self.foo = foo

        obj = Base(100)
        # uncached
        self.assertEqual(get_foo(obj), 100)
        # cached
        self.assertEqual(get_foo(obj), 100)
        obj.__dict__["foo"] = 200
        self.assertEqual(get_foo(obj), 200)

    def test_dict_resplit(self):
        # This causes one resize of the instance dictionary, which should cause
        # it to go from split -> combined -> split.
        class Base:
            def __init__(self):
                self.foo, self.a, self.b = 100, 200, 300
                self.c, self.d, self.e = 400, 500, 600

        obj = Base()
        # uncached
        self.assertEqual(get_foo(obj), 100)
        # cached
        self.assertEqual(get_foo(obj), 100)
        obj.foo = 800
        self.assertEqual(get_foo(obj), 800)

    def test_dict_combined(self):
        class Base:
            def __init__(self, foo):
                self.foo = foo

        obj1 = Base(100)
        # uncached
        self.assertEqual(get_foo(obj1), 100)
        # cached
        self.assertEqual(get_foo(obj1), 100)
        obj2 = Base(200)
        obj2.bar = 300
        # At this point the dictionary should still be split
        obj3 = Base(400)
        obj3.baz = 500
        # Assigning 'baz' should clear the cached key object for Base and leave
        # existing instance dicts in the following states:
        #
        # obj1.__dict__ - Split
        # obj2.__dict__ - Split
        # obj3.__dict__ - Combined
        obj4 = Base(600)
        self.assertEqual(get_foo(obj1), 100)
        self.assertEqual(get_foo(obj2), 200)
        self.assertEqual(get_foo(obj3), 400)
        self.assertEqual(get_foo(obj4), 600)


class SetNonDataDescrAttrTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def set_foo(self, obj, val):
        obj.foo = val

    def setUp(self):
        class Descr:
            def __init__(self, name):
                self.name = name

            def __get__(self, obj, typ):
                return obj.__dict__[self.name]

        self.descr_type = Descr
        self.descr = Descr("foo")

        class Test:
            foo = self.descr

        self.obj = Test()

    def test_set_when_changed_to_data_descr(self):
        # uncached
        self.set_foo(self.obj, 100)
        self.assertEqual(self.obj.foo, 100)

        # cached
        self.set_foo(self.obj, 200)
        self.assertEqual(self.obj.foo, 200)

        # convert into a data descriptor
        def setter(self, obj, val):
            self.invoked = True

        self.descr.__class__.__set__ = setter

        # setter doesn't modify the object, so obj.foo shouldn't change
        self.set_foo(self.obj, 300)
        self.assertEqual(self.obj.foo, 200)
        self.assertTrue(self.descr.invoked)


class GetSetNonDataDescrAttrTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def get_foo(self, obj):
        return obj.foo

    def setUp(self):
        class NonDataDescr:
            def __init__(self, val):
                self.val = val
                self.invoked_count = 0
                self.set_dict = True

            def __get__(self, obj, typ):
                self.invoked_count += 1
                if self.set_dict:
                    obj.__dict__["foo"] = self.val
                return self.val

        self.descr_type = NonDataDescr
        self.descr = NonDataDescr("testing 123")

        class Test:
            foo = self.descr

        self.obj = Test()

    def test_get(self):
        # uncached
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 1)

        # cached; __get__ should not be invoked as there is now a shadowing
        # entry in obj's __dict__
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 1)

        # cached; __get__ should be invoked as there is not a shadowing
        # entry in obj2's __dict__
        obj2 = self.obj.__class__()
        self.assertEqual(self.get_foo(obj2), "testing 123")
        self.assertEqual(self.descr.invoked_count, 2)

    def test_get_when_changed_to_data_descr(self):
        # uncached
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 1)

        # cached; __get__ should not be invoked as there is now a shadowing
        # entry in obj's __dict__
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 1)

        # Convert descriptor into a data descr by modifying its type
        def setter(self, obj, val):
            pass

        self.descr.__class__.__set__ = setter

        # cached; __get__ should be invoked as self.descr is now a data descr
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 2)

    def test_get_when_changed_to_classvar(self):
        # Don't set anything in the instance dict when the descriptor is
        # invoked. This ensures we don't early exit and bottom out into the
        # descriptor case.
        self.descr.set_dict = False

        # uncached
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 1)

        # cached
        self.assertEqual(self.get_foo(self.obj), "testing 123")
        self.assertEqual(self.descr.invoked_count, 2)

        # Convert descriptor into a plain old value by changing the
        # descriptor's type
        class ClassVar:
            pass

        self.descr.__class__ = ClassVar

        # Cached; type check on descriptor's type should fail
        self.assertIs(self.get_foo(self.obj), self.descr)
        self.assertEqual(self.descr.invoked_count, 2)


@unittest.failUnlessJITCompiled
def set_foo(x, val):
    x.foo = val


class DataDescr:
    def __init__(self, val):
        self.val = val
        self.invoked = False

    def __get__(self, obj, typ):
        return self.val

    def __set__(self, obj, val):
        self.invoked = True


class StoreAttrCacheTests(unittest.TestCase):
    def test_data_descr_attached(self):
        class Base:
            def __init__(self, x):
                self.foo = x

        obj = Base(100)

        # Uncached
        set_foo(obj, 200)
        # Cached
        set_foo(obj, 200)
        self.assertEqual(obj.foo, 200)

        # Attaching a data descriptor to the type should invalidate the cache
        # and prevent future caching
        descr = DataDescr(300)
        Base.foo = descr
        set_foo(obj, 200)
        self.assertEqual(obj.foo, 300)
        self.assertTrue(descr.invoked)

        descr.invoked = False
        set_foo(obj, 400)
        self.assertEqual(obj.foo, 300)
        self.assertTrue(descr.invoked)

    def test_swap_split_dict_with_combined(self):
        class Base:
            def __init__(self, x):
                self.foo = x

        obj = Base(100)

        # Uncached
        set_foo(obj, 200)
        # Cached
        set_foo(obj, 200)
        self.assertEqual(obj.foo, 200)

        # At this point obj should have a split dictionary for attribute
        # storage. We're going to swap it out with a combined dictionary
        # and verify that attribute stores still work as expected.
        d = {"foo": 300}
        obj.__dict__ = d
        set_foo(obj, 400)
        self.assertEqual(obj.foo, 400)
        self.assertEqual(d["foo"], 400)

    def test_swap_combined_dict_with_split(self):
        class Base:
            def __init__(self, x):
                self.foo = x

        # Swap out obj's dict with a combined dictionary. Priming the IC
        # for set_foo will result in it expecting a combined dictionary
        # for instances of type Base.
        obj = Base(100)
        obj.__dict__ = {"foo": 100}
        # Uncached
        set_foo(obj, 200)
        # Cached
        set_foo(obj, 200)
        self.assertEqual(obj.foo, 200)

        # obj2 should have a split dictionary used for attribute storage
        # which will result in a cache miss in the IC
        obj2 = Base(300)
        set_foo(obj2, 400)
        self.assertEqual(obj2.foo, 400)

    def test_split_dict_no_slot(self):
        class Base:
            pass

        # obj is a split dict
        obj = Base()
        obj.quox = 42

        # obj1 is no longer split, but the assignment
        # didn't go through _PyObjectDict_SetItem, so the type
        # still has a valid CACHED_KEYS
        obj1 = Base()
        obj1.__dict__["other"] = 100

        # now we try setting foo on obj1, do the set on obj1
        # while setting up the cache, but attempt to create a cache
        # with an invalid val_offset because there's no foo
        # entry in the cached keys.
        set_foo(obj1, 300)
        self.assertEqual(obj1.foo, 300)

        set_foo(obj, 400)
        self.assertEqual(obj1.foo, 300)


class LoadGlobalCacheTests(unittest.TestCase):
    def setUp(self):
        global license, a_global
        try:
            del license
        except NameError:
            pass
        try:
            del a_global
        except NameError:
            pass

    @staticmethod
    def set_global(value):
        global a_global
        a_global = value

    @staticmethod
    @unittest.failUnlessJITCompiled
    def get_global():
        return a_global

    @staticmethod
    def del_global():
        global a_global
        del a_global

    @staticmethod
    def set_license(value):
        global license
        license = value

    @staticmethod
    def del_license():
        global license
        del license

    @unittest.failUnlessJITCompiled
    def test_simple(self):
        global a_global
        self.set_global(123)
        self.assertEqual(a_global, 123)
        self.set_global(456)
        self.assertEqual(a_global, 456)

    @unittest.failUnlessJITCompiled
    def test_shadow_builtin(self):
        self.assertIs(license, builtins.license)
        self.set_license(0xDEADBEEF)
        self.assertIs(license, 0xDEADBEEF)
        self.del_license()
        self.assertIs(license, builtins.license)

    @unittest.failUnlessJITCompiled
    def test_shadow_fake_builtin(self):
        self.assertRaises(NameError, self.get_global)
        builtins.a_global = "poke"
        self.assertEqual(a_global, "poke")
        self.set_global("override poke")
        self.assertEqual(a_global, "override poke")
        self.del_global()
        self.assertEqual(a_global, "poke")
        # We don't support DELETE_ATTR yet.
        delattr(builtins, "a_global")
        self.assertRaises(NameError, self.get_global)

    class prefix_str(str):
        def __new__(ty, prefix, value):
            s = super().__new__(ty, value)
            s.prefix = prefix
            return s

        def __hash__(self):
            return hash(self.prefix + self)

        def __eq__(self, other):
            return (self.prefix + self) == other

    @unittest.failUnlessJITCompiled
    def test_weird_key_in_globals(self):
        global a_global
        self.assertRaises(NameError, self.get_global)
        globals()[self.prefix_str("a_glo", "bal")] = "a value"
        self.assertEqual(a_global, "a value")
        self.assertEqual(self.get_global(), "a value")

    class MyGlobals(dict):
        def __getitem__(self, key):
            if key == "knock_knock":
                return "who's there?"
            return super().__getitem__(key)

    @with_globals(MyGlobals())
    def return_knock_knock(self):
        return knock_knock

    def test_dict_subclass_globals(self):
        self.assertEqual(self.return_knock_knock(), "who's there?")

    @unittest.failUnlessJITCompiled
    def _test_unwatch_builtins(self):
        self.set_global("hey")
        self.assertEqual(self.get_global(), "hey")
        builtins.__dict__[42] = 42

    def test_unwatch_builtins(self):
        try:
            self._test_unwatch_builtins()
        finally:
            del builtins.__dict__[42]

    @contextmanager
    def temp_sys_path(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            _orig_sys_modules = sys.modules
            sys.modules = _orig_sys_modules.copy()
            _orig_sys_path = sys.path[:]
            sys.path.insert(0, tmpdir)
            try:
                yield Path(tmpdir)
            finally:
                sys.path[:] = _orig_sys_path
                sys.modules = _orig_sys_modules

    def test_preload_side_effect_modifies_globals(self):
        with self.temp_sys_path() as tmp:
            (tmp / "tmp_a.py").write_text(
                dedent(
                    """
                    from __future__ import lazy_imports
                    from tmp_b import B

                    A = 1

                    def get_a():
                        return A + B

                    """
                ),
                encoding="utf8",
            )
            (tmp / "tmp_b.py").write_text(
                dedent(
                    """
                    import tmp_a

                    tmp_a.A = 2

                    B = 3
                    """
                ),
                encoding="utf8",
            )
            if cinderjit:
                cinderjit.clear_runtime_stats()
            import tmp_a

            # What happens on the first call is kinda undefined in principle
            # given lazy imports; somebody could previously have imported B
            # (not in this specific test, but in principle), or not, so the
            # first call might return 4 or 5. With JIT compilation it will
            # always return 5 because compilation will trigger the lazy import
            # and its side effect. Without the JIT it will return 4 in this
            # test, but we consider this an acceptable side effect of JIT
            # compilation because this code can't in general rely on B never
            # having previously been imported.
            tmp_a.get_a()

            # On the second call the result should undoubtedly be 5 in all
            # circumstances. Even if we compile with the wrong value for A, the
            # guard on the LoadGlobalCached will ensure we deopt and return the
            # right result.
            self.assertEqual(tmp_a.get_a(), 5)
            if cinderjit:
                self.assertTrue(cinderjit.is_jit_compiled(tmp_a.get_a))
                # The real test here is what when the value of a global changes
                # during compilation preload (as it does in this test because
                # the preload bytescan of get_a() first hits A, loads the old
                # value, then hits B, triggers the lazy import and imports
                # tmp_b, causing the value of A to change), we still have time
                # to compile with the correct (new) value and avoid compiling
                # code that will inevitably deopt, and so we should.
                stats = cinderjit.get_and_clear_runtime_stats()
                relevant_deopts = [
                    d for d in stats["deopt"] if d["normal"]["func_qualname"] == "get_a"
                ]
                self.assertEqual(relevant_deopts, [])

    def test_preload_side_effect_makes_globals_unwatchable(self):
        with self.temp_sys_path() as tmp:
            (tmp / "tmp_a.py").write_text(
                dedent(
                    """
                    from __future__ import lazy_imports
                    from tmp_b import B

                    A = 1

                    def get_a():
                        return A + B

                    """
                ),
                encoding="utf8",
            )
            (tmp / "tmp_b.py").write_text(
                dedent(
                    """
                    import tmp_a

                    tmp_a.__dict__[42] = 1
                    tmp_a.A = 2

                    B = 3
                    """
                ),
                encoding="utf8",
            )
            if cinderjit:
                cinderjit.clear_runtime_stats()
            import tmp_a

            tmp_a.get_a()
            self.assertEqual(tmp_a.get_a(), 5)
            if cinderjit:
                self.assertTrue(cinderjit.is_jit_compiled(tmp_a.get_a))

    def test_preload_side_effect_makes_builtins_unwatchable(self):
        with self.temp_sys_path() as tmp:
            (tmp / "tmp_a.py").write_text(
                dedent(
                    """
                    from __future__ import lazy_imports
                    from tmp_b import B

                    def get_a():
                        return max(1, 2) + B

                    """
                ),
                encoding="utf8",
            )
            (tmp / "tmp_b.py").write_text(
                dedent(
                    """
                    __builtins__[42] = 2

                    B = 3
                    """
                ),
                encoding="utf8",
            )
            if cinderjit:
                cinderjit.clear_runtime_stats()
            import tmp_a

            tmp_a.get_a()
            self.assertEqual(tmp_a.get_a(), 5)
            if cinderjit:
                self.assertTrue(cinderjit.is_jit_compiled(tmp_a.get_a))


class ClosureTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def test_cellvar(self):
        a = 1

        def foo():
            return a

        self.assertEqual(foo(), 1)

    @unittest.failUnlessJITCompiled
    def test_two_cellvars(self):
        a = 1
        b = 2

        def g():
            return a + b

        self.assertEqual(g(), 3)

    @unittest.failUnlessJITCompiled
    def test_cellvar_argument(self):
        def foo():
            self.assertEqual(1, 1)

        foo()

    @unittest.failUnlessJITCompiled
    def test_cellvar_argument_modified(self):
        self_ = self

        def foo():
            nonlocal self
            self = 1

        self_.assertIs(self, self_)

        foo()

        self_.assertEqual(self, 1)

    @unittest.failUnlessJITCompiled
    def _cellvar_unbound(self):
        b = a
        a = 1

        def g():
            return a

    def test_cellvar_unbound(self):
        with self.assertRaises(UnboundLocalError) as ctx:
            self._cellvar_unbound()

        self.assertEqual(
            str(ctx.exception), "local variable 'a' referenced before assignment"
        )

    def test_freevars(self):
        x = 1

        @unittest.failUnlessJITCompiled
        def nested():
            return x

        x = 2

        self.assertEqual(nested(), 2)

    def test_freevars_multiple_closures(self):
        def get_func(a):
            @unittest.failUnlessJITCompiled
            def f():
                return a

            return f

        f1 = get_func(1)
        f2 = get_func(2)

        self.assertEqual(f1(), 1)
        self.assertEqual(f2(), 2)

    def test_nested_func(self):
        @unittest.failUnlessJITCompiled
        def add(a, b):
            return a + b

        self.assertEqual(add(1, 2), 3)
        self.assertEqual(add("eh", "bee"), "ehbee")

    @staticmethod
    def make_adder(a):
        @unittest.failUnlessJITCompiled
        def add(b):
            return a + b

        return add

    def test_nested_func_with_closure(self):
        add_3 = self.make_adder(3)
        add_7 = self.make_adder(7)

        self.assertEqual(add_3(10), 13)
        self.assertEqual(add_7(12), 19)
        self.assertEqual(add_3(add_7(-100)), -90)
        with self.assertRaises(TypeError):
            add_3("ok")

    def test_nested_func_with_different_globals(self):
        @unittest.failUnlessJITCompiled
        @with_globals({"A_GLOBAL_CONSTANT": 0xDEADBEEF})
        def return_global():
            return A_GLOBAL_CONSTANT

        self.assertEqual(return_global(), 0xDEADBEEF)

        return_other_global = with_globals({"A_GLOBAL_CONSTANT": 0xFACEB00C})(
            return_global
        )
        self.assertEqual(return_other_global(), 0xFACEB00C)

        self.assertEqual(return_global(), 0xDEADBEEF)
        self.assertEqual(return_other_global(), 0xFACEB00C)

    def test_nested_func_outlives_parent(self):
        @unittest.failUnlessJITCompiled
        def nested(x):
            @unittest.failUnlessJITCompiled
            def inner(y):
                return x + y

            return inner

        nested_ref = weakref.ref(nested)
        add_5 = nested(5)
        nested = None
        self.assertIsNone(nested_ref())
        self.assertEqual(add_5(10), 15)


class TempNameTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _tmp_name(self, a, b):
        tmp1 = "hello"
        c = a + b
        return tmp1

    def test_tmp_name(self):
        self.assertEqual(self._tmp_name(1, 2), "hello")

    @unittest.failUnlessJITCompiled
    def test_tmp_name2(self):
        v0 = 5
        self.assertEqual(v0, 5)


class DummyContainer:
    def __len__(self):
        raise Exception("hello!")


class ExceptionInConditional(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def doit(self, x):
        if x:
            return 1
        return 2

    def test_exception_thrown_in_conditional(self):
        with self.assertRaisesRegex(Exception, "hello!"):
            self.doit(DummyContainer())


class JITCompileCrasherRegressionTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _fstring(self, flag, it1, it2):
        for a in it1:
            for b in it2:
                if flag:
                    return f"{a}"

    def test_fstring_no_fmt_spec_in_nested_loops_and_if(self):
        self.assertEqual(self._fstring(True, [1], [1]), "1")

    @unittest.failUnlessJITCompiled
    async def _sharedAwait(self, x, y, z):
        return await (x() if y else z())

    def test_shared_await(self):
        async def zero():
            return 0

        async def one():
            return 1

        with self.assertRaises(StopIteration) as exc:
            self._sharedAwait(zero, True, one).send(None)
        self.assertEqual(exc.exception.value, 0)

        with self.assertRaises(StopIteration) as exc:
            self._sharedAwait(zero, False, one).send(None)
        self.assertEqual(exc.exception.value, 1)


class DelObserver:
    def __init__(self, id, cb):
        self.id = id
        self.cb = cb

    def __del__(self):
        self.cb(self.id)


class UnwindStateTests(unittest.TestCase):
    DELETED = []

    def setUp(self):
        self.DELETED.clear()
        self.addCleanup(lambda: self.DELETED.clear())

    def get_del_observer(self, id):
        return DelObserver(id, lambda i: self.DELETED.append(i))

    @unittest.failUnlessJITCompiled
    def _copied_locals(self, a):
        b = c = a
        raise RuntimeError()

    def test_copied_locals_in_frame(self):
        try:
            self._copied_locals("hello")
        except RuntimeError as re:
            f_locals = re.__traceback__.tb_next.tb_frame.f_locals
            self.assertEqual(
                f_locals, {"self": self, "a": "hello", "b": "hello", "c": "hello"}
            )

    @unittest.failUnlessJITCompiled
    def _raise_with_del_observer_on_stack(self):
        for x in (1 for i in [self.get_del_observer(1)]):
            raise RuntimeError()

    def test_decref_stack_objects(self):
        """Items on stack should be decrefed on unwind."""
        try:
            self._raise_with_del_observer_on_stack()
        except RuntimeError:
            deleted = list(self.DELETED)
        else:
            self.fail("should have raised RuntimeError")
        self.assertEqual(deleted, [1])

    @unittest.failUnlessJITCompiled
    def _raise_with_del_observer_on_stack_and_cell_arg(self):
        for x in (self for i in [self.get_del_observer(1)]):
            raise RuntimeError()

    def test_decref_stack_objs_with_cell_args(self):
        # Regression test for a JIT bug in which the unused locals slot for a
        # local-which-is-a-cell would end up getting populated on unwind with
        # some unrelated stack object, preventing it from being decrefed.
        try:
            self._raise_with_del_observer_on_stack_and_cell_arg()
        except RuntimeError:
            deleted = list(self.DELETED)
        else:
            self.fail("should have raised RuntimeError")
        self.assertEqual(deleted, [1])


class ImportTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def test_import_name(self):
        import math

        self.assertEqual(int(math.pow(1, 2)), 1)

    @unittest.failUnlessJITCompiled
    def _fail_to_import_name(self):
        import non_existent_module

    def test_import_name_failure(self):
        with self.assertRaises(ModuleNotFoundError):
            self._fail_to_import_name()

    @unittest.failUnlessJITCompiled
    def test_import_from(self):
        from math import pow as math_pow

        self.assertEqual(int(math_pow(1, 2)), 1)

    @unittest.failUnlessJITCompiled
    def _fail_to_import_from(self):
        from math import non_existent_attr

    def test_import_from_failure(self):
        with self.assertRaises(ImportError):
            self._fail_to_import_from()


class RaiseTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _jitRaise(self, exc):
        raise exc

    @unittest.failUnlessJITCompiled
    def _jitRaiseCause(self, exc, cause):
        raise exc from cause

    @unittest.failUnlessJITCompiled
    def _jitReraise(self):
        raise

    def test_raise_type(self):
        with self.assertRaises(ValueError):
            self._jitRaise(ValueError)

    def test_raise_value(self):
        with self.assertRaises(ValueError) as exc:
            self._jitRaise(ValueError(1))
        self.assertEqual(exc.exception.args, (1,))

    def test_raise_with_cause(self):
        cause = ValueError(2)
        cause_tb_str = f"{cause.__traceback__}"
        with self.assertRaises(ValueError) as exc:
            self._jitRaiseCause(ValueError(1), cause)
        self.assertIs(exc.exception.__cause__, cause)
        self.assertEqual(f"{exc.exception.__cause__.__traceback__}", cause_tb_str)

    def test_reraise(self):
        original_raise = ValueError(1)
        with self.assertRaises(ValueError) as exc:
            try:
                raise original_raise
            except ValueError:
                self._jitReraise()
        self.assertIs(exc.exception, original_raise)

    def test_reraise_of_nothing(self):
        with self.assertRaises(RuntimeError) as exc:
            self._jitReraise()
        self.assertEqual(exc.exception.args, ("No active exception to reraise",))


class GeneratorsTest(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _f1(self):
        yield 1

    def test_basic_operation(self):
        g = self._f1()
        self.assertEqual(g.send(None), 1)
        with self.assertRaises(StopIteration) as exc:
            g.send(None)
        self.assertIsNone(exc.exception.value)

    @unittest.failUnlessJITCompiled
    def _f2(self):
        yield 1
        yield 2
        return 3

    def test_multi_yield_and_return(self):
        g = self._f2()
        self.assertEqual(g.send(None), 1)
        self.assertEqual(g.send(None), 2)
        with self.assertRaises(StopIteration) as exc:
            g.send(None)
        self.assertEqual(exc.exception.value, 3)

    @unittest.failUnlessJITCompiled
    def _f3(self):
        a = yield 1
        b = yield 2
        return a + b

    def test_receive_values(self):
        g = self._f3()
        self.assertEqual(g.send(None), 1)
        self.assertEqual(g.send(100), 2)
        with self.assertRaises(StopIteration) as exc:
            g.send(1000)
        self.assertEqual(exc.exception.value, 1100)

    @unittest.failUnlessJITCompiled
    def _f4(self, a):
        yield a
        yield a
        return a

    def test_one_arg(self):
        g = self._f4(10)
        self.assertEqual(g.send(None), 10)
        self.assertEqual(g.send(None), 10)
        with self.assertRaises(StopIteration) as exc:
            g.send(None)
        self.assertEqual(exc.exception.value, 10)

    @unittest.failUnlessJITCompiled
    def _f5(
        self, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16
    ):
        v = (
            yield a1
            + a2
            + a3
            + a4
            + a5
            + a6
            + a7
            + a8
            + a9
            + a10
            + a11
            + a12
            + a13
            + a14
            + a15
            + a16
        )
        a1 <<= v
        a2 <<= v
        a3 <<= v
        a4 <<= v
        a5 <<= v
        a6 <<= v
        a7 <<= v
        a8 <<= v
        a9 <<= v
        a10 <<= v
        a11 <<= v
        a12 <<= v
        a13 <<= v
        a14 <<= v
        a15 <<= v
        a16 <<= v
        v = (
            yield a1
            + a2
            + a3
            + a4
            + a5
            + a6
            + a7
            + a8
            + a9
            + a10
            + a11
            + a12
            + a13
            + a14
            + a15
            + a16
        )
        a1 <<= v
        a2 <<= v
        a3 <<= v
        a4 <<= v
        a5 <<= v
        a6 <<= v
        a7 <<= v
        a8 <<= v
        a9 <<= v
        a10 <<= v
        a11 <<= v
        a12 <<= v
        a13 <<= v
        a14 <<= v
        a15 <<= v
        a16 <<= v
        return (
            a1
            + a2
            + a3
            + a4
            + a5
            + a6
            + a7
            + a8
            + a9
            + a10
            + a11
            + a12
            + a13
            + a14
            + a15
            + a16
        )

    def test_save_all_registers_and_spill(self):
        g = self._f5(
            0x1,
            0x2,
            0x4,
            0x8,
            0x10,
            0x20,
            0x40,
            0x80,
            0x100,
            0x200,
            0x400,
            0x800,
            0x1000,
            0x2000,
            0x4000,
            0x8000,
        )
        self.assertEqual(g.send(None), 0xFFFF)
        self.assertEqual(g.send(1), 0xFFFF << 1)
        with self.assertRaises(StopIteration) as exc:
            g.send(2)
        self.assertEqual(exc.exception.value, 0xFFFF << 3)

    def test_for_loop_driven(self):
        l = []
        for x in self._f2():
            l.append(x)
        self.assertEqual(l, [1, 2])

    @unittest.failUnlessJITCompiled
    def _f6(self):
        i = 0
        while i < 1000:
            i = yield i

    def test_many_iterations(self):
        g = self._f6()
        self.assertEqual(g.send(None), 0)
        for i in range(1, 1000):
            self.assertEqual(g.send(i), i)
        with self.assertRaises(StopIteration) as exc:
            g.send(1000)
        self.assertIsNone(exc.exception.value)

    def _f_raises(self):
        raise ValueError

    @unittest.failUnlessJITCompiled
    def _f7(self):
        self._f_raises()
        yield 1

    def test_raise(self):
        g = self._f7()
        with self.assertRaises(ValueError):
            g.send(None)

    def test_throw_into_initial_yield(self):
        g = self._f1()
        with self.assertRaises(ValueError):
            g.throw(ValueError)

    def test_throw_into_yield(self):
        g = self._f2()
        self.assertEqual(g.send(None), 1)
        with self.assertRaises(ValueError):
            g.throw(ValueError)

    def test_close_on_initial_yield(self):
        g = self._f1()
        g.close()

    def test_close_on_yield(self):
        g = self._f2()
        self.assertEqual(g.send(None), 1)
        g.close()

    @unittest.failUnlessJITCompiled
    def _f8(self, a):
        x += yield a

    def test_do_not_deopt_before_initial_yield(self):
        g = self._f8(1)
        with self.assertRaises(UnboundLocalError):
            g.send(None)

    @unittest.failUnlessJITCompiled
    def _f9(self, a):
        yield
        return a

    def test_incref_args(self):
        class X:
            pass

        g = self._f9(X())
        g.send(None)
        with self.assertRaises(StopIteration) as exc:
            g.send(None)
        self.assertIsInstance(exc.exception.value, X)

    @unittest.failUnlessJITCompiled
    def _f10(self, X):
        x = X()
        yield weakref.ref(x)
        return x

    def test_gc_traversal(self):
        class X:
            pass

        g = self._f10(X)

        weak_ref_x = g.send(None)
        self.assertIn(weak_ref_x(), gc.get_objects())
        referrers = gc.get_referrers(weak_ref_x())
        self.assertEqual(len(referrers), 1)
        if unittest.case.CINDERJIT_ENABLED:
            self.assertIs(referrers[0], g)
        else:
            self.assertIs(referrers[0], g.gi_frame)
        with self.assertRaises(StopIteration):
            g.send(None)

    def test_resuming_in_another_thread(self):
        g = self._f1()

        def thread_function(g):
            self.assertEqual(g.send(None), 1)
            with self.assertRaises(StopIteration):
                g.send(None)

        t = threading.Thread(target=thread_function, args=(g,))
        t.start()
        t.join()

    def test_release_data_on_discard(self):
        o = object()
        base_count = sys.getrefcount(o)
        g = self._f9(o)
        self.assertEqual(sys.getrefcount(o), base_count + 1)
        del g
        self.assertEqual(sys.getrefcount(o), base_count)

    @unittest.failUnlessJITCompiled
    def _f12(self, g):
        a = yield from g
        return a

    def test_yield_from_generator(self):
        g = self._f12(self._f2())
        self.assertEqual(g.send(None), 1)
        self.assertEqual(g.send(None), 2)
        with self.assertRaises(StopIteration) as exc:
            g.send(None)
        self.assertEqual(exc.exception.value, 3)

    def test_yield_from_iterator(self):
        g = self._f12([1, 2])
        self.assertEqual(g.send(None), 1)
        self.assertEqual(g.send(None), 2)
        with self.assertRaises(StopIteration):
            g.send(None)

    def test_yield_from_forwards_raise_down(self):
        def f():
            try:
                yield 1
            except ValueError:
                return 2
            return 3

        g = self._f12(f())
        self.assertEqual(g.send(None), 1)
        with self.assertRaises(StopIteration) as exc:
            g.throw(ValueError)
        self.assertEqual(exc.exception.value, 2)

    def test_yield_from_forwards_raise_up(self):
        def f():
            raise ValueError
            yield 1

        g = self._f12(f())
        with self.assertRaises(ValueError):
            g.send(None)

    def test_yield_from_passes_raise_through(self):
        g = self._f12(self._f2())
        self.assertEqual(g.send(None), 1)
        with self.assertRaises(ValueError):
            g.throw(ValueError)

    def test_yield_from_forwards_close_down(self):
        saw_close = False

        def f():
            nonlocal saw_close
            try:
                yield 1
            except GeneratorExit:
                saw_close = True
                return 2

        g = self._f12(f())
        self.assertEqual(g.send(None), 1)
        g.close()
        self.assertTrue(saw_close)

    def test_yield_from_passes_close_through(self):
        g = self._f12(self._f2())
        self.assertEqual(g.send(None), 1)
        g.close()

    def test_assert_on_yield_from_coro(self):
        async def coro():
            pass

        c = coro()
        with self.assertRaises(TypeError) as exc:
            self._f12(c).send(None)
        self.assertEqual(
            str(exc.exception),
            "cannot 'yield from' a coroutine object in a non-coroutine generator",
        )

        # Suppress warning
        c.close()

    def test_gen_freelist(self):
        """Exercise making a JITted generator with gen_data memory off the freelist."""
        # make and dealloc a small coro, which will put its memory area on the freelist
        sc = self.small_coro()
        with self.assertRaises(StopIteration):
            sc.send(None)
        del sc
        # run another coro to verify we didn't put a bad pointer on the freelist
        sc2 = self.small_coro()
        with self.assertRaises(StopIteration):
            sc2.send(None)
        del sc2
        # make a big coro and then deallocate it, bypassing the freelist
        bc = self.big_coro()
        with self.assertRaises(StopIteration):
            bc.send(None)
        del bc

    @unittest.failUnlessJITCompiled
    async def big_coro(self):
        # This currently results in a max spill size of ~100, but that could
        # change with JIT register allocation improvements. This test is only
        # testing what it intends to as long as the max spill size of this
        # function is greater than jit::kMinGenSpillWords. Ideally we'd assert
        # that in the test, but neither value is introspectable from Python.
        return dict(
            a=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            b=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            c=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            d=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            e=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            f=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            g=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
            h=dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9),
        )

    @unittest.failUnlessJITCompiled
    async def small_coro(self):
        return 1

    def test_generator_globals(self):
        val1 = "a value"
        val2 = "another value"
        gbls = {"A_GLOBAL": val1}

        @with_globals(gbls)
        def gen():
            yield A_GLOBAL
            yield A_GLOBAL

        g = gen()
        self.assertIs(g.__next__(), val1)
        gbls["A_GLOBAL"] = val2
        del gbls
        self.assertIs(g.__next__(), val2)
        with self.assertRaises(StopIteration):
            g.__next__()


class GeneratorFrameTest(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def gen1(self):
        a = 1
        yield a
        a = 2
        yield a

    def test_access_before_send(self):
        g = self.gen1()
        f = g.gi_frame
        self.assertEqual(next(g), 1)
        self.assertEqual(g.gi_frame, f)
        self.assertEqual(next(g), 2)
        self.assertEqual(g.gi_frame, f)

    def test_access_after_send(self):
        g = self.gen1()
        self.assertEqual(next(g), 1)
        f = g.gi_frame
        self.assertEqual(next(g), 2)
        self.assertEqual(g.gi_frame, f)

    @unittest.failUnlessJITCompiled
    def gen2(self):
        me = yield
        f = me.gi_frame
        yield f
        yield 10

    def test_access_while_running(self):
        g = self.gen2()
        next(g)
        f = g.send(g)
        self.assertEqual(f, g.gi_frame)
        next(g)


class CoroutinesTest(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    async def _f1(self):
        return 1

    @unittest.failUnlessJITCompiled
    async def _f2(self, await_target):
        return await await_target

    def test_basic_coroutine(self):
        c = self._f2(self._f1())
        with self.assertRaises(StopIteration) as exc:
            c.send(None)
        self.assertEqual(exc.exception.value, 1)

    def test_cannot_await_coro_already_awaiting_on_a_sub_iterator(self):
        class DummyAwaitable:
            def __await__(self):
                return iter([1])

        c = self._f2(DummyAwaitable())
        self.assertEqual(c.send(None), 1)
        with self.assertRaises(RuntimeError) as exc:
            self._f2(c).send(None)
        self.assertEqual(str(exc.exception), "coroutine is being awaited already")

    def test_works_with_asyncio(self):
        try:
            asyncio.run(self._f2(asyncio.sleep(0.1)))
        finally:
            # This is needed to avoid an "environment changed" error
            asyncio.set_event_loop_policy(None)

    @unittest.failUnlessJITCompiled
    @asyncio.coroutine
    def _f3(self):
        yield 1
        return 2

    def test_pre_async_coroutine(self):
        c = self._f3()
        self.assertEqual(c.send(None), 1)
        with self.assertRaises(StopIteration) as exc:
            c.send(None)
        self.assertEqual(exc.exception.value, 2)

    @staticmethod
    @unittest.failUnlessJITCompiled
    async def _use_async_with(mgr_type):
        async with mgr_type():
            pass

    def test_bad_awaitable_in_with(self):
        class BadAEnter:
            def __aenter__(self):
                pass

            async def __aexit__(self, exc, ty, tb):
                pass

        class BadAExit:
            async def __aenter__(self):
                pass

            def __aexit__(self, exc, ty, tb):
                pass

        with self.assertRaisesRegex(
            TypeError,
            "'async with' received an object from __aenter__ "
            "that does not implement __await__: NoneType",
        ):
            asyncio.run(self._use_async_with(BadAEnter))
        with self.assertRaisesRegex(
            TypeError,
            "'async with' received an object from __aexit__ "
            "that does not implement __await__: NoneType",
        ):
            asyncio.run(self._use_async_with(BadAExit))

    class FakeFuture:
        def __init__(self, obj):
            self._obj = obj

        def __await__(self):
            i = iter([self._obj])
            self._obj = None
            return i

    @unittest.skipUnlessCinderJITEnabled("Exercises JIT-specific bug")
    def test_jit_coro_awaits_interp_coro(self):
        @cinderjit.jit_suppress
        async def eager_suspend(suffix):
            await self.FakeFuture("hello, " + suffix)

        @unittest.failUnlessJITCompiled
        async def jit_coro():
            await eager_suspend("bob")

        coro = jit_coro()
        v1 = coro.send(None)
        with self.assertRaises(StopIteration):
            coro.send(None)
        self.assertEqual(v1, "hello, bob")

    def assert_already_awaited(self, coro):
        with self.assertRaisesRegex(RuntimeError, "coroutine is being awaited already"):
            asyncio.run(coro)

    def test_already_awaited_coroutine_in_try_except(self):
        """Except blocks should execute when a coroutine is already awaited"""

        async def f():
            await asyncio.sleep(0.1)

        executed_except_block = False

        async def runner():
            nonlocal executed_except_block
            coro = f()
            loop = asyncio.get_running_loop()
            t = loop.create_task(coro)
            try:
                await asyncio.sleep(0)
                await coro
            except RuntimeError:
                executed_except_block = True
                t.cancel()
                raise

        self.assert_already_awaited(runner())
        self.assertTrue(executed_except_block)

    def test_already_awaited_coroutine_in_try_finally(self):
        """Finally blocks should execute when a coroutine is already awaited"""

        async def f():
            await asyncio.sleep(0.1)

        executed_finally_block = False

        async def runner():
            nonlocal executed_finally_block
            coro = f()
            loop = asyncio.get_running_loop()
            t = loop.create_task(coro)
            try:
                await asyncio.sleep(0)
                await coro
            finally:
                executed_finally_block = True
                t.cancel()

        self.assert_already_awaited(runner())
        self.assertTrue(executed_finally_block)

    def test_already_awaited_coroutine_in_try_except_finally(self):
        """Except and finally blocks should execute when a coroutine is already
        awaited.
        """

        async def f():
            await asyncio.sleep(0.1)

        executed_except_block = False
        executed_finally_block = False

        async def runner():
            nonlocal executed_except_block, executed_finally_block
            coro = f()
            loop = asyncio.get_running_loop()
            t = loop.create_task(coro)
            try:
                await asyncio.sleep(0)
                await coro
            except RuntimeError:
                executed_except_block = True
                raise
            finally:
                executed_finally_block = True
                t.cancel()

        self.assert_already_awaited(runner())
        self.assertTrue(executed_except_block)
        self.assertTrue(executed_finally_block)


class EagerCoroutineDispatch(StaticTestBase):
    def _assert_awaited_flag_seen(self, async_f_under_test):
        awaited_capturer = _testcapi.TestAwaitedCall()
        self.assertIsNone(awaited_capturer.last_awaited())
        coro = async_f_under_test(awaited_capturer)
        # TestAwaitedCall doesn't actually return a coroutine. This doesn't
        # matter though because by the time a TypeError is raised we run far
        # enough to know if the awaited flag was passed.
        with self.assertRaisesRegex(
            TypeError, r".*can't be used in 'await' expression"
        ):
            coro.send(None)
        coro.close()
        self.assertTrue(awaited_capturer.last_awaited())
        self.assertIsNone(awaited_capturer.last_awaited())

    def _assert_awaited_flag_not_seen(self, async_f_under_test):
        awaited_capturer = _testcapi.TestAwaitedCall()
        self.assertIsNone(awaited_capturer.last_awaited())
        coro = async_f_under_test(awaited_capturer)
        with self.assertRaises(StopIteration):
            coro.send(None)
        coro.close()
        self.assertFalse(awaited_capturer.last_awaited())
        self.assertIsNone(awaited_capturer.last_awaited())

    @unittest.failUnlessJITCompiled
    async def _call_ex(self, t):
        t(*[1])

    @unittest.failUnlessJITCompiled
    async def _call_ex_awaited(self, t):
        await t(*[1])

    @unittest.failUnlessJITCompiled
    async def _call_ex_kw(self, t):
        t(*[1], **{2: 3})

    @unittest.failUnlessJITCompiled
    async def _call_ex_kw_awaited(self, t):
        await t(*[1], **{2: 3})

    @unittest.failUnlessJITCompiled
    async def _call_method(self, t):
        # https://stackoverflow.com/questions/19476816/creating-an-empty-object-in-python
        o = type("", (), {})()
        o.t = t
        o.t()

    @unittest.failUnlessJITCompiled
    async def _call_method_awaited(self, t):
        o = type("", (), {})()
        o.t = t
        await o.t()

    @unittest.failUnlessJITCompiled
    async def _vector_call(self, t):
        t()

    @unittest.failUnlessJITCompiled
    async def _vector_call_awaited(self, t):
        await t()

    @unittest.failUnlessJITCompiled
    async def _vector_call_kw(self, t):
        t(a=1)

    @unittest.failUnlessJITCompiled
    async def _vector_call_kw_awaited(self, t):
        await t(a=1)

    def test_call_ex(self):
        self._assert_awaited_flag_not_seen(self._call_ex)

    def test_call_ex_awaited(self):
        self._assert_awaited_flag_seen(self._call_ex_awaited)

    def test_call_ex_kw(self):
        self._assert_awaited_flag_not_seen(self._call_ex_kw)

    def test_call_ex_kw_awaited(self):
        self._assert_awaited_flag_seen(self._call_ex_kw_awaited)

    def test_call_method(self):
        self._assert_awaited_flag_not_seen(self._call_method)

    def test_call_method_awaited(self):
        self._assert_awaited_flag_seen(self._call_method_awaited)

    def test_vector_call(self):
        self._assert_awaited_flag_not_seen(self._vector_call)

    def test_vector_call_awaited(self):
        self._assert_awaited_flag_seen(self._vector_call_awaited)

    def test_vector_call_kw(self):
        self._assert_awaited_flag_not_seen(self._vector_call_kw)

    def test_vector_call_kw_awaited(self):
        self._assert_awaited_flag_seen(self._vector_call_kw_awaited)

    def test_invoke_function(self):
        codestr = f"""
        def x() -> None:
            pass

        async def await_x() -> None:
            await x()

        async def call_x() -> None:
            c = x()
        """
        c = self.compile(codestr, StaticCodeGenerator, modname="test_invoke_function")
        await_x = self.find_code(c, "await_x")
        self.assertInBytecode(
            await_x, "INVOKE_FUNCTION", (("test_invoke_function", "x"), 0)
        )
        call_x = self.find_code(c, "call_x")
        self.assertInBytecode(
            call_x, "INVOKE_FUNCTION", (("test_invoke_function", "x"), 0)
        )
        with self.in_module(codestr) as mod:
            mod.x = _testcapi.TestAwaitedCall()
            self.assertIsInstance(mod.x, _testcapi.TestAwaitedCall)
            self.assertIsNone(mod.x.last_awaited())
            coro = mod.await_x()
            with self.assertRaisesRegex(
                TypeError, r".*can't be used in 'await' expression"
            ):
                coro.send(None)
            coro.close()
            self.assertTrue(mod.x.last_awaited())
            self.assertIsNone(mod.x.last_awaited())
            coro = mod.call_x()
            with self.assertRaises(StopIteration):
                coro.send(None)
            coro.close()
            self.assertFalse(mod.x.last_awaited())
            if cinderjit:
                self.assertTrue(cinderjit.is_jit_compiled(mod.await_x))
                self.assertTrue(cinderjit.is_jit_compiled(mod.call_x))

    def test_invoke_method(self):
        codestr = f"""
        class X:
            def x(self) -> None:
                pass

        async def await_x() -> None:
            await X().x()

        async def call_x() -> None:
            X().x()
        """
        c = self.compile(codestr, StaticCodeGenerator, modname="test_invoke_method")
        await_x = self.find_code(c, "await_x")
        self.assertInBytecode(
            await_x, "INVOKE_METHOD", (("test_invoke_method", "X", "x"), 0)
        )
        call_x = self.find_code(c, "call_x")
        self.assertInBytecode(
            call_x, "INVOKE_METHOD", (("test_invoke_method", "X", "x"), 0)
        )
        with self.in_module(codestr) as mod:
            awaited_capturer = mod.X.x = _testcapi.TestAwaitedCall()
            self.assertIsNone(awaited_capturer.last_awaited())
            coro = mod.await_x()
            with self.assertRaisesRegex(
                TypeError, r".*can't be used in 'await' expression"
            ):
                coro.send(None)
            coro.close()
            self.assertTrue(awaited_capturer.last_awaited())
            self.assertIsNone(awaited_capturer.last_awaited())
            coro = mod.call_x()
            with self.assertRaises(StopIteration):
                coro.send(None)
            coro.close()
            self.assertFalse(awaited_capturer.last_awaited())
            if cinderjit:
                self.assertTrue(cinderjit.is_jit_compiled(mod.await_x))
                self.assertTrue(cinderjit.is_jit_compiled(mod.call_x))

        async def y():
            await DummyAwaitable()

    def test_async_yielding(self):
        class DummyAwaitable:
            def __await__(self):
                return iter([1, 2])

        coro = self._vector_call_awaited(DummyAwaitable)
        self.assertEqual(coro.send(None), 1)
        self.assertEqual(coro.send(None), 2)


class AsyncGeneratorsTest(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    async def _f1(self, awaitable):
        x = yield 1
        yield x
        await awaitable

    def test_basic_coroutine(self):
        class DummyAwaitable:
            def __await__(self):
                return iter([3])

        async_gen = self._f1(DummyAwaitable())

        # Step 1: move through "yield 1"
        async_itt1 = async_gen.asend(None)
        with self.assertRaises(StopIteration) as exc:
            async_itt1.send(None)
        self.assertEqual(exc.exception.value, 1)

        # Step 2: send in and receive out 2 via "yield x"
        async_itt2 = async_gen.asend(2)
        with self.assertRaises(StopIteration) as exc:
            async_itt2.send(None)
        self.assertEqual(exc.exception.value, 2)

        # Step 3: yield of "3" from DummyAwaitable
        async_itt3 = async_gen.asend(None)
        self.assertEqual(async_itt3.send(None), 3)

        # Step 4: complete
        with self.assertRaises(StopAsyncIteration):
            async_itt3.send(None)

    @unittest.failUnlessJITCompiled
    async def _f2(self, asyncgen):
        res = []
        async for x in asyncgen:
            res.append(x)
        return res

    def test_for_iteration(self):
        async def asyncgen():
            yield 1
            yield 2

        self.assertEqual(asyncio.run(self._f2(asyncgen())), [1, 2])

    def _assertExceptionFlowsThroughYieldFrom(self, exc):
        tb_prev = None
        tb = exc.__traceback__
        while tb.tb_next:
            tb_prev = tb
            tb = tb.tb_next
        instrs = [x for x in dis.get_instructions(tb_prev.tb_frame.f_code)]
        self.assertEqual(instrs[tb_prev.tb_lasti // 2].opname, "YIELD_FROM")

    def test_for_exception(self):
        async def asyncgen():
            yield 1
            raise ValueError

        # Can't use self.assertRaises() as this clears exception tracebacks
        try:
            asyncio.run(self._f2(asyncgen()))
        except ValueError as e:
            self._assertExceptionFlowsThroughYieldFrom(e)
        else:
            self.fail("Expected ValueError to be raised")

    @unittest.failUnlessJITCompiled
    async def _f3(self, asyncgen):
        return [x async for x in asyncgen]

    def test_comprehension(self):
        async def asyncgen():
            yield 1
            yield 2

        self.assertEqual(asyncio.run(self._f3(asyncgen())), [1, 2])

    def test_comprehension_exception(self):
        async def asyncgen():
            yield 1
            raise ValueError

        # Can't use self.assertRaises() as this clears exception tracebacks
        try:
            asyncio.run(self._f3(asyncgen()))
        except ValueError as e:
            self._assertExceptionFlowsThroughYieldFrom(e)
        else:
            self.fail("Expected ValueError to be raised")


class Err1(Exception):
    pass


class Err2(Exception):
    pass


class ExceptionHandlingTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def try_except(self, func):
        try:
            func()
        except:
            return True
        return False

    def test_raise_and_catch(self):
        def f():
            raise Exception("hello")

        self.assertTrue(self.try_except(f))

        def g():
            pass

        self.assertFalse(self.try_except(g))

    @unittest.failUnlessJITCompiled
    def catch_multiple(self, func):
        try:
            func()
        except Err1:
            return 1
        except Err2:
            return 2

    def test_multiple_except_blocks(self):
        def f():
            raise Err1("err1")

        self.assertEqual(self.catch_multiple(f), 1)

        def g():
            raise Err2("err2")

        self.assertEqual(self.catch_multiple(g), 2)

    @unittest.failUnlessJITCompiled
    def reraise(self, func):
        try:
            func()
        except:
            raise

    def test_reraise(self):
        def f():
            raise Exception("hello")

        with self.assertRaisesRegex(Exception, "hello"):
            self.reraise(f)

    @unittest.failUnlessJITCompiled
    def try_except_in_loop(self, niters, f):
        for i in range(niters):
            try:
                try:
                    f(i)
                except Err2:
                    pass
            except Err1:
                break
        return i

    def test_try_except_in_loop(self):
        def f(i):
            if i == 10:
                raise Err1("hello")

        self.assertEqual(self.try_except_in_loop(20, f), 10)

    @unittest.failUnlessJITCompiled
    def nested_try_except(self, f):
        try:
            try:
                try:
                    f()
                except:
                    raise
            except:
                raise
        except:
            return 100

    def test_nested_try_except(self):
        def f():
            raise Exception("hello")

        self.assertEqual(self.nested_try_except(f), 100)

    @unittest.failUnlessJITCompiled
    def try_except_in_generator(self, f):
        try:
            yield f(0)
            yield f(1)
            yield f(2)
        except:
            yield 123

    def test_except_in_generator(self):
        def f(i):
            if i == 1:
                raise Exception("hello")
            return

        g = self.try_except_in_generator(f)
        next(g)
        self.assertEqual(next(g), 123)

    @unittest.failUnlessJITCompiled
    def try_finally(self, should_raise):
        result = None
        try:
            if should_raise:
                raise Exception("testing 123")
        finally:
            result = 100
        return result

    def test_try_finally(self):
        self.assertEqual(self.try_finally(False), 100)
        with self.assertRaisesRegex(Exception, "testing 123"):
            self.try_finally(True)

    @unittest.failUnlessJITCompiled
    def try_except_finally(self, should_raise):
        result = None
        try:
            if should_raise:
                raise Exception("testing 123")
        except Exception:
            result = 200
        finally:
            if result is None:
                result = 100
        return result

    def test_try_except_finally(self):
        self.assertEqual(self.try_except_finally(False), 100)
        self.assertEqual(self.try_except_finally(True), 200)

    @unittest.failUnlessJITCompiled
    def return_in_finally(self, v):
        try:
            pass
        finally:
            return v

    @unittest.failUnlessJITCompiled
    def return_in_finally2(self, v):
        try:
            return v
        finally:
            return 100

    @unittest.failUnlessJITCompiled
    def return_in_finally3(self, v):
        try:
            1 / 0
        finally:
            return v

    @unittest.failUnlessJITCompiled
    def return_in_finally4(self, v):
        try:
            return 100
        finally:
            try:
                1 / 0
            finally:
                return v

    def test_return_in_finally(self):
        self.assertEqual(self.return_in_finally(100), 100)
        self.assertEqual(self.return_in_finally2(200), 100)
        self.assertEqual(self.return_in_finally3(300), 300)
        self.assertEqual(self.return_in_finally4(400), 400)

    @unittest.failUnlessJITCompiled
    def break_in_finally_after_return(self, x):
        for count in [0, 1]:
            count2 = 0
            while count2 < 20:
                count2 += 10
                try:
                    return count + count2
                finally:
                    if x:
                        break
        return "end", count, count2

    @unittest.failUnlessJITCompiled
    def break_in_finally_after_return2(self, x):
        for count in [0, 1]:
            for count2 in [10, 20]:
                try:
                    return count + count2
                finally:
                    if x:
                        break
        return "end", count, count2

    def test_break_in_finally_after_return(self):
        self.assertEqual(self.break_in_finally_after_return(False), 10)
        self.assertEqual(self.break_in_finally_after_return(True), ("end", 1, 10))
        self.assertEqual(self.break_in_finally_after_return2(False), 10)
        self.assertEqual(self.break_in_finally_after_return2(True), ("end", 1, 10))

    @unittest.failUnlessJITCompiled
    def continue_in_finally_after_return(self, x):
        count = 0
        while count < 100:
            count += 1
            try:
                return count
            finally:
                if x:
                    continue
        return "end", count

    @unittest.failUnlessJITCompiled
    def continue_in_finally_after_return2(self, x):
        for count in [0, 1]:
            try:
                return count
            finally:
                if x:
                    continue
        return "end", count

    def test_continue_in_finally_after_return(self):
        self.assertEqual(self.continue_in_finally_after_return(False), 1)
        self.assertEqual(self.continue_in_finally_after_return(True), ("end", 100))
        self.assertEqual(self.continue_in_finally_after_return2(False), 0)
        self.assertEqual(self.continue_in_finally_after_return2(True), ("end", 1))

    @unittest.failUnlessJITCompiled
    def return_in_loop_in_finally(self, x):
        try:
            for _ in [1, 2, 3]:
                if x:
                    return x
        finally:
            pass
        return 100

    def test_return_in_loop_in_finally(self):
        self.assertEqual(self.return_in_loop_in_finally(True), True)
        self.assertEqual(self.return_in_loop_in_finally(False), 100)

    @unittest.failUnlessJITCompiled
    def conditional_return_in_finally(self, x, y, z):
        try:
            if x:
                return x
            if y:
                return y
        finally:
            pass
        return z

    def test_conditional_return_in_finally(self):
        self.assertEqual(self.conditional_return_in_finally(100, False, False), 100)
        self.assertEqual(self.conditional_return_in_finally(False, 200, False), 200)
        self.assertEqual(self.conditional_return_in_finally(False, False, 300), 300)

    @unittest.failUnlessJITCompiled
    def nested_finally(self, x):
        try:
            if x:
                return x
        finally:
            try:
                y = 10
            finally:
                z = y
        return z

    def test_nested_finally(self):
        self.assertEqual(self.nested_finally(100), 100)
        self.assertEqual(self.nested_finally(False), 10)


class UnpackSequenceTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _unpack_arg(self, seq, which):
        a, b, c, d = seq
        if which == "a":
            return a
        if which == "b":
            return b
        if which == "c":
            return c
        return d

    @unittest.failUnlessJITCompiled
    def _unpack_ex_arg(self, seq, which):
        a, b, *c, d = seq
        if which == "a":
            return a
        if which == "b":
            return b
        if which == "c":
            return c
        return d

    def test_unpack_tuple(self):
        self.assertEqual(self._unpack_arg(("eh", "bee", "see", "dee"), "b"), "bee")
        self.assertEqual(self._unpack_arg((3, 2, 1, 0), "c"), 1)

    @unittest.skipUnderCinderJITNotFullFrame("deopt not supported in no-frame mode")
    def test_unpack_tuple_wrong_size(self):
        with self.assertRaises(ValueError):
            self._unpack_arg((1, 2, 3, 4, 5), "a")

    @unittest.skipUnderCinderJITNotFullFrame("deopt not supported in no-frame mode")
    def test_unpack_list(self):
        self.assertEqual(self._unpack_arg(["one", "two", "three", "four"], "a"), "one")

    @unittest.skipUnderCinderJITNotFullFrame("deopt not supported in no-frame mode")
    def test_unpack_gen(self):
        def gen():
            yield "first"
            yield "second"
            yield "third"
            yield "fourth"

        self.assertEqual(self._unpack_arg(gen(), "d"), "fourth")

    @unittest.failUnlessJITCompiled
    def _unpack_not_iterable(self):
        (a, b, *c) = 1

    @unittest.failUnlessJITCompiled
    def _unpack_insufficient_values(self):
        (a, b, *c) = [1]

    @unittest.failUnlessJITCompiled
    def _unpack_insufficient_values_after(self):
        (a, *b, c, d) = [1, 2]

    @unittest.skipUnderCinderJITNotFullFrame("deopt not supported in no-frame mode")
    def test_unpack_ex(self):
        with self.assertRaises(TypeError):
            self._unpack_not_iterable()
        with self.assertRaises(ValueError):
            self._unpack_insufficient_values()
        with self.assertRaises(ValueError):
            self._unpack_insufficient_values_after()

        seq = [1, 2, 3, 4, 5, 6]
        self.assertEqual(self._unpack_ex_arg(seq, "a"), 1)
        self.assertEqual(self._unpack_ex_arg(seq, "b"), 2)
        self.assertEqual(self._unpack_ex_arg(seq, "c"), [3, 4, 5])
        self.assertEqual(self._unpack_ex_arg(seq, "d"), 6)


class DeleteSubscrTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _delit(self, container, key):
        del container[key]

    def test_builtin_types(self):
        l = [1, 2, 3]
        self._delit(l, 1)
        self.assertEqual(l, [1, 3])

        d = {"foo": 1, "bar": 2}
        self._delit(d, "foo")
        self.assertEqual(d, {"bar": 2})

    def test_custom_type(self):
        class CustomContainer:
            def __init__(self):
                self.item = None

            def __delitem__(self, item):
                self.item = item

        c = CustomContainer()
        self._delit(c, "foo")
        self.assertEqual(c.item, "foo")

    def test_missing_key(self):
        d = {"foo": 1}
        with self.assertRaises(KeyError):
            self._delit(d, "bar")

    def test_custom_error(self):
        class CustomContainer:
            def __delitem__(self, item):
                raise Exception("testing 123")

        c = CustomContainer()
        with self.assertRaisesRegex(Exception, "testing 123"):
            self._delit(c, "foo")


class DeleteFastTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def _del(self):
        x = 2
        del x

    @unittest.failUnlessJITCompiled
    def _del_arg(self, a):
        del a

    @unittest.failUnlessJITCompiled
    def _del_and_raise(self):
        x = 2
        del x
        return x

    @unittest.failUnlessJITCompiled
    def _del_arg_and_raise(self, a):
        del a
        return a

    @unittest.failUnlessJITCompiled
    def _del_ex_no_raise(self):
        try:
            return min(1, 2)
        except Exception as e:
            pass

    @unittest.failUnlessJITCompiled
    def _del_ex_raise(self):
        try:
            raise Exception()
        except Exception as e:
            pass
        return e

    def test_del_local(self):
        self.assertEqual(self._del(), None)

    def test_del_arg(self):
        self.assertEqual(self._del_arg(42), None)

    def test_del_and_raise(self):
        with self.assertRaises(NameError):
            self._del_and_raise()

    def test_del_arg_and_raise(self):
        with self.assertRaises(NameError):
            self.assertEqual(self._del_arg_and_raise(42), None)

    def test_del_ex_no_raise(self):
        self.assertEqual(self._del_ex_no_raise(), 1)

    def test_del_ex_raise(self):
        with self.assertRaises(NameError):
            self.assertEqual(self._del_ex_raise(), 42)


class KeywordOnlyArgTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def f1(self, *, val=10):
        return val

    @unittest.failUnlessJITCompiled
    def f2(self, which, *, y=10, z=20):
        if which == 0:
            return y
        elif which == 1:
            return z
        return which

    @unittest.failUnlessJITCompiled
    def f3(self, which, *, y, z=20):
        if which == 0:
            return y
        elif which == 1:
            return z
        return which

    @unittest.failUnlessJITCompiled
    def f4(self, which, *, y, z=20, **kwargs):
        if which == 0:
            return y
        elif which == 1:
            return z
        elif which == 2:
            return kwargs
        return which

    def test_kwonly_arg_passed_as_positional(self):
        msg = "takes 1 positional argument but 2 were given"
        with self.assertRaisesRegex(TypeError, msg):
            self.f1(100)
        msg = "takes 2 positional arguments but 3 were given"
        with self.assertRaisesRegex(TypeError, msg):
            self.f3(0, 1)

    def test_kwonly_args_with_kwdefaults(self):
        self.assertEqual(self.f1(), 10)
        self.assertEqual(self.f1(val=20), 20)
        self.assertEqual(self.f2(0), 10)
        self.assertEqual(self.f2(0, y=20), 20)
        self.assertEqual(self.f2(1), 20)
        self.assertEqual(self.f2(1, z=30), 30)

    def test_kwonly_args_without_kwdefaults(self):
        self.assertEqual(self.f3(0, y=10), 10)
        self.assertEqual(self.f3(1, y=10), 20)
        self.assertEqual(self.f3(1, y=10, z=30), 30)

    def test_kwonly_args_and_varkwargs(self):
        self.assertEqual(self.f4(0, y=10), 10)
        self.assertEqual(self.f4(1, y=10), 20)
        self.assertEqual(self.f4(1, y=10, z=30, a=40), 30)
        self.assertEqual(self.f4(2, y=10, z=30, a=40, b=50), {"a": 40, "b": 50})


class ClassA:
    z = 100
    x = 41

    def g(self, a):
        return 42 + a

    @classmethod
    def cls_g(cls, a):
        return 100 + a


class ClassB(ClassA):
    def f(self, a):
        return super().g(a=a)

    def f_2arg(self, a):
        return super(ClassB, self).g(a=a)

    @classmethod
    def cls_f(cls, a):
        return super().cls_g(a=a)

    @classmethod
    def cls_f_2arg(cls, a):
        return super(ClassB, cls).cls_g(a=a)

    @property
    def x(self):
        return super().x + 1

    @property
    def x_2arg(self):
        return super(ClassB, self).x + 1


class SuperAccessTest(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def test_super_method(self):
        self.assertEqual(ClassB().f(1), 43)
        self.assertEqual(ClassB().f_2arg(1), 43)
        self.assertEqual(ClassB.cls_f(99), 199)
        self.assertEqual(ClassB.cls_f_2arg(99), 199)

    @unittest.failUnlessJITCompiled
    def test_super_method_kwarg(self):
        self.assertEqual(ClassB().f(1), 43)
        self.assertEqual(ClassB().f_2arg(1), 43)
        self.assertEqual(ClassB.cls_f(1), 101)
        self.assertEqual(ClassB.cls_f_2arg(1), 101)

    @unittest.failUnlessJITCompiled
    def test_super_attr(self):
        self.assertEqual(ClassB().x, 42)
        self.assertEqual(ClassB().x_2arg, 42)


class RegressionTests(StaticTestBase):
    # Detects an issue in the backend where the Store instruction generated 32-
    # bit memory writes for 64-bit constants.
    def test_store_of_64bit_immediates(self):
        codestr = f"""
            from __static__ import int64, box
            class Cint64:
                def __init__(self):
                    self.a: int64 = 0x5555555555555555

            def testfunc():
                c = Cint64()
                c.a = 2
                return box(c.a) == 2
        """
        with self.in_module(codestr) as mod:
            testfunc = mod.testfunc
            self.assertTrue(testfunc())

            if cinderjit:
                self.assertTrue(cinderjit.is_jit_compiled(testfunc))


@unittest.skipUnlessCinderJITEnabled("Requires cinderjit module")
class CinderJitModuleTests(StaticTestBase):
    def test_bad_disable(self):
        with self.assertRaises(TypeError):
            cinderjit.disable(1, 2)

        with self.assertRaises(TypeError):
            cinderjit.disable(None)

    def test_jit_force_normal_frame_changes_flags(self):
        def x():
            pass

        self.assertEqual(x.__code__.co_flags & CO_NORMAL_FRAME, 0)

        forced_x = cinderjit.jit_force_normal_frame(x)

        self.assertEqual(x.__code__.co_flags & CO_NORMAL_FRAME, CO_NORMAL_FRAME)

    def test_jit_force_normal_frame_raises_on_invalid_arg(self):
        with self.assertRaises(TypeError):
            cinderjit.jit_force_normal_frame(None)

    def test_jit_suppress(self):
        @cinderjit.jit_suppress
        def x():
            pass

        self.assertEqual(x.__code__.co_flags & CO_SUPPRESS_JIT, CO_SUPPRESS_JIT)

    def test_jit_suppress_static(self):
        codestr = f"""
            import cinderjit

            @cinderjit.jit_suppress
            def f():
                return True

            def g():
                return True
        """
        with self.in_module(codestr) as mod:
            f = mod.f
            g = mod.g
            self.assertTrue(f())
            self.assertTrue(g())

            self.assertFalse(cinderjit.is_jit_compiled(f))
            self.assertTrue(cinderjit.is_jit_compiled(g))


@jit_suppress
def _inner(*args, **kwargs):
    return kwargs


@unittest.failUnlessJITCompiled
def _outer(args, kwargs):
    return _inner(*args, **kwargs)


class GetFrameInFinalizer:
    def __del__(self):
        sys._getframe()


def _create_getframe_cycle():
    a = {"fg": GetFrameInFinalizer()}
    b = {"a": a}
    a["b"] = b
    return a


class TestException(Exception):
    pass


class GetFrameTests(unittest.TestCase):
    @unittest.failUnlessJITCompiled
    def f1(self, leaf):
        return self.f2(leaf)

    @unittest.failUnlessJITCompiled
    def f2(self, leaf):
        return self.f3(leaf)

    @unittest.failUnlessJITCompiled
    def f3(self, leaf):
        return leaf()

    def assert_frames(self, frame, names):
        for name in names:
            self.assertEqual(frame.f_code.co_name, name)
            frame = frame.f_back

    @unittest.failUnlessJITCompiled
    def simple_getframe(self):
        return sys._getframe()

    def test_simple_getframe(self):
        stack = ["simple_getframe", "f3", "f2", "f1", "test_simple_getframe"]
        frame = self.f1(self.simple_getframe)
        self.assert_frames(frame, stack)

    @unittest.failUnlessJITCompiled
    def consecutive_getframe(self):
        f1 = sys._getframe()
        f2 = sys._getframe()
        return f1, f2

    def test_consecutive_getframe(self):
        stack = ["consecutive_getframe", "f3", "f2", "f1", "test_consecutive_getframe"]
        frame1, frame2 = self.f1(self.consecutive_getframe)
        self.assert_frames(frame1, stack)
        # Make sure the second call to sys._getframe doesn't rematerialize
        # frames
        for _ in range(4):
            self.assertTrue(frame1 is frame2)
            frame1 = frame1.f_back
            frame2 = frame2.f_back

    @unittest.failUnlessJITCompiled
    def getframe_then_deopt(self):
        f = sys._getframe()
        try:
            raise Exception("testing 123")
        except:
            return f

    def test_getframe_then_deopt(self):
        # Make sure we correctly unlink a materialized frame after its function
        # deopts into the interpreter
        stack = ["getframe_then_deopt", "f3", "f2", "f1", "test_getframe_then_deopt"]
        frame = self.f1(self.getframe_then_deopt)
        self.assert_frames(frame, stack)

    @unittest.failUnlessJITCompiled
    def getframe_in_except(self):
        try:
            raise Exception("testing 123")
        except:
            return sys._getframe()

    def test_getframe_after_deopt(self):
        stack = ["getframe_in_except", "f3", "f2", "f1", "test_getframe_after_deopt"]
        frame = self.f1(self.getframe_in_except)
        self.assert_frames(frame, stack)

    class FrameGetter:
        def __init__(self, box):
            self.box = box

        def __del__(self):
            self.box[0] = sys._getframe()

    def do_raise(self, x):
        # Clear reference held by frame in the traceback that gets created with
        # the exception
        del x
        raise Exception("testing 123")

    @unittest.failUnlessJITCompiled
    def getframe_in_dtor_during_deopt(self):
        ref = ["notaframe"]
        try:
            self.do_raise(self.FrameGetter(ref))
        except:
            return ref[0]

    def test_getframe_in_dtor_during_deopt(self):
        # Test that we can correctly walk the stack in the middle of deopting
        frame = self.f1(self.getframe_in_dtor_during_deopt)
        stack = [
            "__del__",
            "getframe_in_dtor_during_deopt",
            "f3",
            "f2",
            "f1",
            "test_getframe_in_dtor_during_deopt",
        ]
        self.assert_frames(frame, stack)

    @unittest.failUnlessJITCompiled
    def getframe_in_dtor_after_deopt(self):
        ref = ["notaframe"]
        frame_getter = self.FrameGetter(ref)
        try:
            raise Exception("testing 123")
        except:
            return ref

    def test_getframe_in_dtor_after_deopt(self):
        # Test that we can correctly walk the stack in the interpreter after
        # deopting but before returning to the caller
        frame = self.f1(self.getframe_in_dtor_after_deopt)[0]
        stack = ["__del__", "f3", "f2", "f1", "test_getframe_in_dtor_after_deopt"]
        self.assert_frames(frame, stack)

    @jit_suppress
    def test_frame_allocation_race(self):
        # This test exercises a race condition that can occur in the
        # interpreted function prologue between when its frame is
        # allocated and when its set as `tstate->frame`.
        #
        # When a frame is allocated its f_back field is set to point to
        # tstate->frame. Shortly thereafter, tstate->frame is set to the
        # newly allocated frame by the interpreter loop. There is an implicit
        # assumption that tstate->frame will not change between when the
        # frame is allocated and when it is set to tstate->frame.  That
        # assumption is invalid when the JIT is executing in shadow-frame mode.
        #
        # tstate->frame may change if the Python stack is materialized between
        # when the new frame is allocated and when its set as
        # tstate->frame. The window of time is very small, but it's
        # possible. We must ensure that if tstate->frame changes, the newly
        # allocated frame's f_back is updated to point to it. Otherwise, we can
        # end up with a missing frame on the Python stack. To see why, consider
        # the following scenario.
        #
        # Terms:
        #
        # - shadow stack - The call stack of _PyShadowFrame objects beginning
        #   at tstate->shadow_frame.
        # - pyframe stack - The call stack of PyFrameObject objects beginning
        #   at tstate->frame.
        #
        # T0:
        #
        # At time T0, the stacks look like:
        #
        # Shadow Stack        PyFrame Stack
        # ------------        ------------
        # f0                  f0
        # f1
        #
        # - f0 is interpreted and has called f2.
        # - f1 is jit-compiled and is running in shadow-frame mode. The stack
        #   hasn't been materialized, so there is no entry for f1 on the
        #   PyFrame stack.
        #
        # T1:
        #
        # At time T1, f1 calls f2. f2 is interpreted and has a variable keyword
        # parameter (**kwargs). The call to f2 enters PyEval_EvalCodeWithName.
        # PyEval_EvalCodeWithName allocates a new PyFrameObject, p, to run
        # f2. At this point the stacks still look the same, however, notice
        # that p->f_back points to f0, not f1.
        #
        # Shadow Stack        PyFrame Stack
        # ------------        ------------
        # f0                  f0              <--- p->f_back points to f1
        # f1
        #
        # T2:
        #
        # At time T2, PyEval_EvalCodeWithName has allocated the PyFrameObject,
        # p, for f2, and allocates a new dictionary for the variable keyword
        # parameter. The dictionary allocation triggers GC. During GC an object
        # is collected with a finalizer that materializes the stack. The most
        # common way for this to happen is through an unawaited coroutine. The
        # coroutine's finalizer will call _PyErr_WarnUnawaitedCoroutine which
        # materializes the stack.
        #
        # Notice that the stacks match after materialization, however,
        # p->f_back still points to f0.
        #
        # Shadow Stack        PyFrame Stack
        # ------------        ------------
        # f0                  f0              <--- p->f_back points to f0
        # f1                  f1
        #
        # T3:
        #
        # At time T3, control has transferred from PyEval_EvalCodeWithName
        # to the interpreter loop. The interpreter loop has set tstate->frame
        # to the frame it was passed, p. Now the stacks are mismatched:
        #
        # Shadow Stack        PyFrame Stack
        # ------------        ------------
        # f0                  f0
        # f1                  f2
        # f2
        #
        # T4:
        #
        # At time T4, f2 finishes execution and returns into f1.
        #
        # Shadow Stack        PyFrame Stack
        # ------------        ------------
        # f0                  f0
        # f1
        #
        # T5:
        #
        # At time T5, f1 finishes executing and attempts to return. Since the
        # stack was materialized, it expects to find a PyFrameObject for
        # f1 at the top of the PyFrame stack and aborts when it does not.
        #
        # The test below exercises this scenario.
        thresholds = gc.get_threshold()
        # Initialize zombie frame for _inner (called by _outer). The zombie
        # frame will be used the next time _inner is called. This avoids a
        # trip to the allocator that could trigger GC.
        args = []
        kwargs = {"foo": 1}
        _outer(args, kwargs)
        # Reset counts to zero. This allows us to set a threshold for
        # the first generation that will trigger collection when the keyword
        # dictionary is allocated in PyEval_EvalCodeWithName.
        gc.collect()
        # Create cyclic garbage that will materialize the Python stack when
        # it is collected
        _create_getframe_cycle()
        try:
            # JITRT_CallFunctionEx constructs a new keyword dictionary and args
            # tuple. PyEval_EvalCodeWithName does as well.
            gc.set_threshold(4)
            _outer(args, kwargs)
        finally:
            gc.set_threshold(*thresholds)


class GetGenFrameDuringThrowTest(unittest.TestCase):
    def setUp(self) -> None:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        self.loop = loop

    def tearDown(self):
        self.loop.close()
        asyncio.set_event_loop_policy(None)

    @unittest.failUnlessJITCompiled
    async def outer_propagates_exc(self, inner):
        return await inner

    @unittest.failUnlessJITCompiled
    async def outer_handles_exc(self, inner):
        try:
            await inner
        except TestException:
            return 123

    async def inner(self, fut, outer_box):
        try:
            await fut
        except TestException:
            outer_coro = outer_box[0]
            outer_coro.cr_frame
            raise

    def run_test(self, outer_func):
        box = [None]
        fut = asyncio.Future()
        inner = self.inner(fut, box)
        outer = outer_func(inner)
        box[0] = outer
        outer.send(None)
        return outer.throw(TestException())

    def test_unhandled_exc(self):
        with self.assertRaises(TestException):
            self.run_test(self.outer_propagates_exc)

    def test_handled_exc(self):
        with self.assertRaises(StopIteration) as cm:
            self.run_test(self.outer_handles_exc)
        self.assertEqual(cm.exception.value, 123)


class DeleteAttrTests(unittest.TestCase):
    def test_delete_attr(self):
        class C:
            pass

        c = C()
        c.foo = "bar"
        self.assertEqual(c.foo, "bar")
        del c.foo
        with self.assertRaises(AttributeError):
            c.foo

    def test_delete_attr_raises(self):
        class C:
            @property
            def foo(self):
                return "hi"

        c = C()
        self.assertEqual(c.foo, "hi")
        with self.assertRaises(AttributeError):
            del c.foo


_cmp_key = cmp_to_key(lambda x, y: 0)


class OtherTests(unittest.TestCase):
    def test_type_ready(self):
        # T100786119: type(_cmp_key) should have been initialized, or JIT
        # will fail during compilation.
        self.cmp_key = _cmp_key


if __name__ == "__main__":
    unittest.main()
