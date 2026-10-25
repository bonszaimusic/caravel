# SPDX-License-Identifier: GPL-3.0-only
"""Conservative natural-loop discovery for JieLi objdump instructions.

Switches and predicated packets are annotated by target_budget.functions().
Unknown/irreducible flow raises FlowError; callers must retain the old conservative
span estimate rather than silently omit work. Counts are still not CPU cycles.
"""
import re
from collections import deque

BRANCH = re.compile(r'goto -?\d+ <[^>]*: ([0-9a-f]+) >')
RETURN = re.compile(r'\{pc(?:,|\}).*\[sp\+\+\]')


class FlowError(ValueError):
    pass


class Instructions(list):
    def __init__(self):
        super().__init__()
        self.switches = {}
        self.predicates = {}


def natural_loops(insns):
    if not insns:
        return []
    by_addr = {a: i for i, (a, _) in enumerate(insns)}
    succ = []
    for i, (addr, text) in enumerate(insns):
        follow = {i+1} if i+1 < len(insns) else set()
        branch = BRANCH.search(text)
        if RETURN.search(text):
            if re.search(r"\bifs?\s*\(", text):
                raise FlowError("inline conditional return")
            edges = set()
        elif branch:
            target = by_addr.get(int(branch.group(1), 16))
            edges = ({target} if target is not None else set())
            if not text.startswith('goto '):
                edges |= follow
        elif text.startswith(('tbb ', 'tbh ')):
            table = getattr(insns, 'switches', {}).get(addr)
            if not table or any(a not in by_addr for a in table):
                raise FlowError('unresolved jump table')
            edges = {by_addr[a] for a in table}
        elif 'goto' in text:
            raise FlowError('indirect jump')
        else:
            edges = follow
        if re.search(r'\bifs?\s*\(', text) and text.endswith('{'):
            end = getattr(insns, 'predicates', {}).get(addr)
            if end not in by_addr:
                raise FlowError('unresolved predicated packet')
            edges.add(by_addr[end])
        succ.append(edges)

    # Basic blocks make the dominator calculation small even for a large ISR.
    leaders = {0}
    for i, edges in enumerate(succ):
        if edges != ({i+1} if i+1 < len(insns) else set()):
            leaders.update(edges)
            if i+1 < len(insns):
                leaders.add(i+1)
    starts = sorted(leaders)
    owner = [0]*len(insns)
    members = []
    for b, start in enumerate(starts):
        end = starts[b+1] if b+1 < len(starts) else len(insns)
        members.append(set(range(start, end)))
        for i in range(start, end):
            owner[i] = b
    graph = [{owner[j] for j in succ[max(nodes)]} for nodes in members]
    reachable, todo = set(), [0]
    while todo:
        b = todo.pop()
        if b not in reachable:
            reachable.add(b)
            todo.extend(graph[b])
    pred = [set() for _ in members]
    for b in reachable:
        for c in graph[b]:
            pred[c].add(b)
    all_bits = sum(1 << b for b in reachable)
    dom = [all_bits for _ in members]
    dom[0] = 1
    changed = True
    while changed:
        changed = False
        for b in sorted(reachable - {0}):
            bits = all_bits
            for p in pred[b]:
                bits &= dom[p]
            bits |= 1 << b
            if bits != dom[b]:
                dom[b] = bits
                changed = True

    back = {(b, h) for b in reachable for h in graph[b] if dom[b] & (1 << h)}
    # Removing all dominator backedges must leave a DAG in reducible flow.
    # Otherwise natural loops alone would undercount an irreducible cycle.
    indegree = {b: 0 for b in reachable}
    for b in reachable:
        for h in graph[b]:
            if (b, h) not in back:
                indegree[h] += 1
    queue = deque(b for b, n in indegree.items() if n == 0)
    visited = 0
    while queue:
        b = queue.popleft(); visited += 1
        for h in graph[b]:
            if (b, h) not in back:
                indegree[h] -= 1
                if indegree[h] == 0:
                    queue.append(h)
    if visited != len(reachable):
        raise FlowError('irreducible cycle')

    loops = {}
    for tail, head in back:
        nodes = {head, tail}; todo = [tail] if tail != head else []
        while todo:
            b = todo.pop()
            for p in pred[b]:
                if p not in nodes:
                    nodes.add(p); todo.append(p)
        loops.setdefault(head, set()).update(nodes)
    return [{insns[i][0] for b in nodes for i in members[b]} for nodes in loops.values()]
