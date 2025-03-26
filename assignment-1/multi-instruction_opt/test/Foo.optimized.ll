; ModuleID = 'Foo.optimized.bc'
source_filename = "Foo.ll"

define dso_local i32 @foo(i32 noundef %0) {
  %a = add i32 %0, 1
  ret i32 %0
  
}
