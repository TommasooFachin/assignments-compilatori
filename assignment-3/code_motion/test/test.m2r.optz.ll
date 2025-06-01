; ModuleID = 'test.m2r.ll'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local void @test_code_motion() #0 {
  %1 = add nsw i32 3, 2
  %2 = add nsw i32 0, 1
  %3 = srem i32 0, 2
  br label %4

4:                                                ; preds = %16, %0
  %.01 = phi i32 [ 0, %0 ], [ %12, %16 ]
  %.0 = phi i32 [ 10, %0 ], [ %.1, %16 ]
  %5 = add nsw i32 %.01, 1
  %6 = icmp eq i32 0, 5
  br i1 %6, label %7, label %8

7:                                                ; preds = %4
  br label %8

8:                                                ; preds = %7, %4
  %9 = icmp eq i32 %3, 0
  br i1 %9, label %10, label %11

10:                                               ; preds = %8
  br label %11

11:                                               ; preds = %10, %8
  %12 = add nsw i32 %2, 1
  %13 = icmp sgt i32 %.0, 5
  br i1 %13, label %14, label %16

14:                                               ; preds = %11
  %15 = add nsw i32 100, 1
  br label %16

16:                                               ; preds = %14, %11
  %.1 = phi i32 [ %15, %14 ], [ %.0, %11 ]
  br label %4
}

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 19.1.7 (++20250114103320+cd708029e0b2-1~exp1~20250114103432.75)"}
