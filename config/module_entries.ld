/* Module entry points the exe's C calls, by the func_ names the PC build gives them stubs for. They live here, not in
   symbols.main.txt, because the module splits read that file too and would cut their own code at these addresses. */
func_800A00CC = 0x800A00CC; /* run_overlay's entry for file 3 */
func_800A1158 = 0x800A1158; /* BATTLE.X's entry */
