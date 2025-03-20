define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  %2 = add nsw i32 %1, 0        ; %2 = %1 + 0 (identità da ottimizzare)
  %3 = add nsw i32 %2, %0       ; %3 = %2 + %0 (somma che include una somma con 0)
  %4 = mul nsw i32 %3, 1        ; %4 = %3 * 1 (identità da ottimizzare)
  %5 = shl i32 %0, 1            ; %5 = %0 << 1
  %6 = sdiv i32 %5, 4           ; %6 = %5 / 4
  %7 = mul nsw i32 %4, %6       ; %7 = %4 * %6
  ret i32 %7
}
