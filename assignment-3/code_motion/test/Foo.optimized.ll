; ModuleID = 'Foo.optimized.bc'
source_filename = "Foo.ll"

define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) {
  %3 = shl i32 %0, 4
  %4 = sub i32 %3, %0
  %5 = shl i32 %0, 4
  %mul3 = mul nsw i32 %0, 5
  %6 = lshr i32 %0, 3
  %div2 = sdiv i32 %0, 7
  %7 = lshr i32 %1, 2
  %tmp1 = add i32 %4, %5
  %tmp2 = sub i32 %tmp1, %6
  %tmp3 = mul i32 %tmp2, %div2
  %result = sdiv i32 %tmp3, %7
  ret i32 %result
}
