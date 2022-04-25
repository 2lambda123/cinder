from .common import StaticTestBase


class DataclassTests(StaticTestBase):
    def test_dataclasses_dataclass_is_dynamic(self) -> None:
        codestr = """
        from dataclasses import dataclass

        @dataclass
        class C:
            x: str

        reveal_type(C)
        """
        self.revealed_type(codestr, "Type[dynamic]")

    def test_static_dataclass_is_not_dynamic(self) -> None:
        for call in [True, False]:
            with self.subTest(call=call):
                insert = "(init=True)" if call else ""
                codestr = f"""
                from __static__ import dataclass

                @dataclass{insert}
                class C:
                    x: str
                """
                self.revealed_type(
                    codestr + "reveal_type(C)", "Type[Exact[<module>.C]]"
                )
                # check we don't call the decorator at runtime
                code = self.compile(codestr)
                self.assertNotInBytecode(code, "LOAD_NAME", "dataclass")

    def test_dataclass_bans_subclassing(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            pass

        class D(C):
            pass
        """
        self.type_error(codestr, "Cannot subclass static dataclasses", at="class D")

    def test_dataclass_basic(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: str
            y: int

        c = C("foo", 42)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c.x, "foo")
            self.assertEqual(mod.c.y, 42)

    def test_dataclass_no_fields(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            pass

        c = C()
        """
        with self.in_strict_module(codestr):
            pass

    def test_dataclass_too_few_args(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: str
            y: int

        C("foo")
        """
        self.type_error(codestr, "expects a value for argument y", at='C("foo")')

    def test_dataclass_too_many_args(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: str
            y: int

        C("foo", 42, "bar")
        """
        self.type_error(codestr, "Mismatched number of args", at='C("foo", 42, "bar")')

    def test_dataclass_incorrect_arg_type(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: str
            y: int

        C("foo", "bar")
        """
        self.type_error(
            codestr, "str received for positional arg 'y', expected int", at='"bar"'
        )

    def test_default_value(self) -> None:
        codestr = """
        from __static__ import dataclass

        def generate_str() -> str:
            return "foo"

        @dataclass
        class C:
            x: str = generate_str()

        c = C()
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c.x, "foo")

    def test_default_replaced_by_value(self) -> None:
        codestr = """
        from __static__ import dataclass

        def generate_str() -> str:
            return "foo"

        @dataclass
        class C:
            x: str = generate_str()

        c = C("bar")
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.C.x, "foo")
            self.assertEqual(mod.c.x, "bar")

    def test_nondefault_after_default(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: int = 0
            y: str
        """
        self.type_error(
            codestr, "non-default argument y follows default argument", at="dataclass"
        )

    def test_dataclass_with_positional_arg(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass(1)
        class C:
            x: int
        """
        self.type_error(
            codestr, r"dataclass\(\) takes no positional arguments", at="dataclass"
        )

    def test_dataclass_with_non_constant(self) -> None:
        codestr = """
        from __static__ import dataclass

        def thunk() -> bool:
            return True

        @dataclass(init=thunk())
        class C:
            x: int
        """
        self.type_error(
            codestr,
            r"dataclass\(\) arguments must be boolean constants",
            at="dataclass",
        )

    def test_dataclass_with_non_bool(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass(init=1)
        class C:
            x: int
        """
        self.type_error(
            codestr,
            r"dataclass\(\) arguments must be boolean constants",
            at="dataclass",
        )

    def test_dataclass_with_unexpected_kwarg(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass(foo=True)
        class C:
            x: int
        """
        self.type_error(
            codestr,
            r"dataclass\(\) got an unexpected keyword argument 'foo'",
            at="dataclass",
        )

    def test_nondefault_after_default_with_init_false(self) -> None:
        codestr = """
        from __static__ import dataclass
        from typing import Optional

        @dataclass(init=False)
        class C:
            x: int = 0
            y: str

            def __init__(self, y: str) -> None:
                self.y = y

        c1 = C("foo")
        c2 = C("bar")
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c1.x, 0)
            self.assertEqual(mod.c1.y, "foo")

            self.assertEqual(mod.c2.x, 0)
            self.assertEqual(mod.c2.y, "bar")

    def test_field_named_self(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            self: int

        c = C(1)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c.self, 1)

    def test_post_init_checks_args(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: int

            def __post_init__(self, y: int) -> None:
                self.x = 2
        """
        with self.in_strict_module(codestr) as mod:
            self.assertRaisesRegex(
                TypeError,
                r"__post_init__\(\) missing 1 required positional argument: 'y'",
                mod.C,
                1,
            )

    def test_post_init_called(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: int

            def __post_init__(self) -> None:
                self.x = 2

        c = C(1)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c.x, 2)

    def test_unannotated_not_a_field(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x = 0

            def foo(self) -> int:
                return self.x
        """
        with self.in_strict_module(codestr) as mod:
            self.assertNotInBytecode(mod.C.foo, "LOAD_FIELD")

    def test_dataclass_eq_static(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: str
            y: int

        class D:
            def __init__(self, x: str, y: int) -> None:
                self.x = x
                self.y = y

        c1 = C("foo", 1)
        c2 = C("foo", 1)
        c3 = C("bar", 1)
        c4 = C("foo", 2)
        d = D("foo", 1)

        res = (
            c1 == c1,
            c1 == c2,
            c1 == c3,
            c1 == c4,
            c1 == d,
        )
        """
        with self.in_strict_module(codestr) as mod:
            self.assertTupleEqual(mod.res, (True, True, False, False, False))

    def test_dataclass_eq_nonstatic(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: str
            y: int

        class D:
            def __init__(self, x: str, y: int) -> None:
                self.x = x
                self.y = y

        c1 = C("foo", 1)
        c2 = C("foo", 1)
        c3 = C("bar", 1)
        c4 = C("foo", 2)
        d = D("foo", 1)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c1, mod.c1)
            self.assertEqual(mod.c1, mod.c2)
            self.assertNotEqual(mod.c1, mod.c3)
            self.assertNotEqual(mod.c1, mod.c4)
            self.assertNotEqual(mod.c1, mod.d)

    def test_dataclass_eq_does_not_overwrite(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: int

            def __eq__(self, other):
                return True

        c1 = C(1)
        c2 = C(2)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c1, mod.c2)

    def test_dataclass_eq_false_does_not_generate_dunder_eq(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass(eq=False)
        class C:
            x: int

        c1 = C(1)
        c2 = C(1)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.c1, mod.c1)
            self.assertNotEqual(mod.c1, mod.c2)
            self.assertEqual(mod.c2, mod.c2)

    def test_dataclass_eq_with_different_type_delegates_to_other(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass
        class C:
            x: int

        class EqualsEverything:
            def __eq__(self, other) -> bool:
                return True
        """
        with self.in_strict_module(codestr) as mod:
            self.assertEqual(mod.C(1), mod.EqualsEverything())

    def test_order_with_dunder_defined_raises_syntax_error(self) -> None:
        methods = ("__lt__", "__le__", "__gt__", "__ge__")
        for method in methods:
            with self.subTest(method=method):
                codestr = f"""
                from __static__ import dataclass

                @dataclass(order=True)
                class C:
                    x: int
                    y: str

                    def {method}(self, other) -> bool:
                        return False
                """
                self.type_error(
                    codestr,
                    f"Cannot overwrite attribute {method} in class C. "
                    "Consider using functools.total_ordering",
                    at="dataclass",
                )

    def test_comparison_subclass_returns_not_implemented(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass(order=True)
        class C:
            x: int
            y: str
        """
        with self.in_strict_module(codestr) as mod:

            class D(mod.C):
                pass

            c = mod.C(1, "foo")
            d = D(2, "bar")

            self.assertEqual(c.__eq__(d), NotImplemented)
            self.assertEqual(c.__ne__(d), NotImplemented)
            self.assertEqual(c.__lt__(d), NotImplemented)
            self.assertEqual(c.__le__(d), NotImplemented)
            self.assertEqual(c.__gt__(d), NotImplemented)
            self.assertEqual(c.__ge__(d), NotImplemented)

    def test_order_uses_tuple_order(self) -> None:
        codestr = """
        from __static__ import dataclass

        @dataclass(order=True)
        class C:
            x: str
            y: int

        c1 = C("foo", 1)
        c2 = C("foo", 1)
        c3 = C("bar", 1)
        c4 = C("foo", 2)
        """
        with self.in_strict_module(codestr) as mod:
            self.assertFalse(mod.c1 < mod.c2)
            self.assertTrue(mod.c1 <= mod.c2)
            self.assertFalse(mod.c1 > mod.c2)
            self.assertTrue(mod.c1 >= mod.c2)

            self.assertFalse(mod.c1 < mod.c3)
            self.assertFalse(mod.c1 <= mod.c3)
            self.assertTrue(mod.c1 > mod.c3)
            self.assertTrue(mod.c1 >= mod.c3)

            self.assertTrue(mod.c1 < mod.c4)
            self.assertTrue(mod.c1 <= mod.c4)
            self.assertFalse(mod.c1 > mod.c4)
            self.assertFalse(mod.c1 >= mod.c4)
