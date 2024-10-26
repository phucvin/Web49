(module
    (func $fib (export "fib") (param $x i32) (result i32)
        (if (result i32) (i32.lt_s (local.get $x) (i32.const 2))
            (then
                (local.get $x))
            (else
                (i32.add
                    (call $fib (i32.sub (local.get $x) (i32.const 1)))
                    (call $fib (i32.sub (local.get $x) (i32.const 2))))))))

(assert_return (invoke "fib" (i32.const 40)) (i32.const 102334155))