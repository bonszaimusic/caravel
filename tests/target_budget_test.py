# SPDX-License-Identifier: GPL-3.0-only
"""Control-flow regression tests; no target budgets are rewritten."""
import unittest
from unittest.mock import patch, mock_open
import target_budget as budget
from target_loops import Instructions, natural_loops, FlowError


def code(*lines):
    return [(0x100 + 2*i, line) for i, line in enumerate(lines)]


class BudgetTest(unittest.TestCase):
    def test_shared_backward_epilogue_is_not_a_loop(self):
        ins = code('goto 4 <f: 106 >', 'sp += 12', '{pc, r4} = [sp++]',
                   'r0 = r0 / r1 (s)', 'goto -8 <f: 102 >')
        self.assertEqual(budget.cost(ins)['cost'], 0)

    def test_real_loop_retains_wide_division(self):
        ins = code('r0 = 32', 'r5_r4 = r5_r4 / r3 (s)', 'r0 += -1',
                   'if (r0 != 0) goto -6 <f: 102 >', '{pc, r4} = [sp++]')
        self.assertEqual(budget.cost(ins),
                         dict(insns=5, loop=3, div=1, call=0, cost=11))

    def test_backward_join_without_cycle_is_not_nested_work(self):
        ins = code('goto 6 <f: 108 >', 'r2 += 1', 'goto 6 <f: 10c >', 'nop',
                   'r0 = 1', 'goto -10 <f: 102 >', '{pc, r4} = [sp++]')
        self.assertEqual(budget.cost(ins)['cost'], 0)

    def test_true_nested_loops(self):
        ins = code('r0 = 4', 'r1 = 4', 'r2 += 1', 'if (r1 != 0) goto -4 <f: 104 >',
                   'r0 += -1', 'if (r0 != 0) goto -10 <f: 102 >', '{pc, r4} = [sp++]')
        self.assertEqual(budget.cost(ins)['loop'], 5)
        self.assertEqual(budget.cost(ins)['cost'], 11)

    def test_multiple_latches_share_one_loop_header(self):
        ins = code('r0 = 32', 'if (r1 != 0) goto 4 <f: 108 >', 'r0 += -1',
                   'if (r0 != 0) goto -6 <f: 102 >', 'r1 += 1',
                   'if (r0 != 0) goto -10 <f: 102 >', '{pc, r4} = [sp++]')
        self.assertEqual(len(natural_loops(ins)), 1)
        self.assertEqual(budget.cost(ins)['cost'], 5)

    def test_single_instruction_self_loop(self):
        ins = code('if (r0 != 0) goto -2 <f: 100 >', '{pc, r4} = [sp++]')
        self.assertEqual(budget.cost(ins)['cost'], 1)

    def test_irreducible_cycle_fails_closed(self):
        ins = code('if (r0 != 0) goto 2 <f: 104 >', 'goto 2 <f: 106 >',
                   'goto 0 <f: 106 >', 'if (r1 != 0) goto -6 <f: 102 >',
                   'if (r2 != 0) goto -6 <f: 104 >', '{pc, r4} = [sp++]')
        with self.assertRaisesRegex(FlowError, 'irreducible'):
            natural_loops(ins)
        self.assertGreater(budget.cost(ins)['cost'], 0)

    def test_unknown_flow_is_not_silently_accepted(self):
        for first in ['goto r2', 'tbb [r2]', 'tbh [r2]', 'if (r0 != 0) {', 'if (r0 != 0) {pc, r4} = [sp++]']:
            with self.subTest(first=first), self.assertRaises(FlowError):
                natural_loops(code(first, '{pc, r4} = [sp++]'))

    def test_call_is_fallthrough_and_counted_in_a_real_loop(self):
        ins = code('call r1', 'if (r0 != 0) goto -4 <f: 100 >', '{pc, r4} = [sp++]')
        self.assertEqual(budget.cost(ins)['call'], 1)
        self.assertEqual(budget.cost(ins)['loop'], 2)

    def test_parser_switches_and_alignment_padding(self):
        for switch,raw in [('tbb','03'),('tbh','03 00')]:
            listing=f'''drum_render:
 100: 00 00   {switch} [r0]
.GJTIS1:
 102: {raw}   < 3 : 0x3 >
 104: 05 00   < 5 : 0x5 >
 106: 00 00   < 0 : 0x0 >
.GJTIE1:
 108: 00 00   r0 = 1
 10a: 00 00   goto 4 <f: 110 >
 10c: 00 00   r0 = 2
 10e: 00 00   goto 0 <f: 110 >
 110: 00 00   {{pc, r4}} = [sp++]
'''
            with self.subTest(switch=switch), patch('builtins.open',mock_open(read_data=listing)):
                ins=budget.functions('fixture')['drum_render']
                self.assertEqual(ins.switches[0x100],{0x108,0x10c})
                self.assertEqual(natural_loops(ins),[])

    def test_unresolved_table_target_fails_closed(self):
        ins=Instructions();ins.extend(code('tbb [r0]','{pc, r4} = [sp++]'))
        ins.switches[0x100]={0xdead}
        with self.assertRaises(FlowError):natural_loops(ins)

    def test_parser_predicated_return_keeps_false_path_reachable(self):
        listing='''drum_render:
 100: 00 00   if (r0 != 0) {
 102: 00 00       {pc, r4} = [sp++]
                        } <annotation>
 104: 00 00   r1 += -1
 106: 00 00   if (r1 != 0) goto -4 <f: 104 >
 108: 00 00   {pc, r4} = [sp++]
'''
        with patch('builtins.open',mock_open(read_data=listing)):
            ins=budget.functions('fixture')['drum_render']
        self.assertEqual(ins.predicates,{0x100:0x104})
        self.assertEqual(budget.cost(ins)['cost'],2)

    def test_empty_straight_line_and_formula(self):
        self.assertEqual(budget.cost([])['cost'],0)
        ins=code('r0 = r1 / r2','{pc, r4} = [sp++]')
        self.assertEqual(budget.cost(ins)['cost'],0)
        self.assertEqual(budget.cost(ins,True)['cost'],2)


if __name__=='__main__':unittest.main()
