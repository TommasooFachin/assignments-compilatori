define dso_local i32 @foo(i32 noundef %0, i32 noundef %1) #0 {
  ; Caso 1: 15 * x (dovrebbe diventare (x << 4) - x)
  %mul1 = mul nsw i32 %0, 15
  
  ; Caso 2: 16 * x (dovrebbe diventare x << 4)
  %mul2 = mul nsw i32 %0, 16
  
  ; Caso 3: 5 * x (non è 2^n -1, nessuna ottimizzazione)
  %mul3 = mul nsw i32 %0, 5
  
  ; Caso 4: x / 8 (dovrebbe diventare x >> 3)
  %div1 = sdiv i32 %0, 8
  
  ; Caso 5: x / 7 (non è potenza di 2, nessuna ottimizzazione)
  %div2 = sdiv i32 %0, 7
  
  ; Caso 6: y / 4 (dovrebbe diventare y >> 2)
  %div3 = sdiv i32 %1, 4
  
  ; Mix di operazioni per testare effetti collaterali
  %tmp1 = add i32 %mul1, %mul2
  %tmp2 = sub i32 %tmp1, %div1
  %tmp3 = mul i32 %tmp2, %div2
  %result = sdiv i32 %tmp3, %div3  ; Divisione con operando non-costante
  
  ret i32 %result
}