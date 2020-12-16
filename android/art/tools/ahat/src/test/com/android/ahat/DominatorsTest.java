/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.ahat;

import com.android.ahat.dominators.DominatorsComputation;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import org.junit.Test;
import static org.junit.Assert.assertEquals;

public class DominatorsTest {
  private static class Node implements DominatorsComputation.Node {
    public String name;
    public List<Node> depends = new ArrayList<Node>();
    public Node dominator;
    private Object dominatorsComputationState;

    public Node(String name) {
      this.name = name;
    }

    public void computeDominators() {
      DominatorsComputation.computeDominators(this);
    }

    public String toString() {
      return name;
    }

    @Override
    public void setDominatorsComputationState(Object state) {
      dominatorsComputationState = state;
    }

    @Override
    public Object getDominatorsComputationState() {
      return dominatorsComputationState;
    }

    @Override
    public Collection<Node> getReferencesForDominators() {
      return depends;
    }

    @Override
    public void setDominator(DominatorsComputation.Node dominator) {
      this.dominator = (Node)dominator;
    }
  }

  @Test
  public void singleNode() {
    // --> n
    // Trivial case.
    Node n = new Node("n");
    n.computeDominators();
  }

  @Test
  public void parentWithChild() {
    // --> parent --> child
    // The child node is dominated by the parent.
    Node parent = new Node("parent");
    Node child = new Node("child");
    parent.depends = Arrays.asList(child);

    parent.computeDominators();
    assertEquals(parent, child.dominator);
  }

  @Test
  public void reachableTwoWays() {
    //            /-> right -->\
    // --> parent               child
    //            \-> left --->/
    // The child node can be reached either by right or by left.
    Node parent = new Node("parent");
    Node right = new Node("right");
    Node left = new Node("left");
    Node child = new Node("child");
    parent.depends = Arrays.asList(left, right);
    right.depends = Arrays.asList(child);
    left.depends = Arrays.asList(child);

    parent.computeDominators();
    assertEquals(parent, left.dominator);
    assertEquals(parent, right.dominator);
    assertEquals(parent, child.dominator);
  }

  @Test
  public void reachableDirectAndIndirect() {
    //            /-> right -->\
    // --> parent  -----------> child
    // The child node can be reached either by right or parent.
    Node parent = new Node("parent");
    Node right = new Node("right");
    Node child = new Node("child");
    parent.depends = Arrays.asList(right, child);
    right.depends = Arrays.asList(child);

    parent.computeDominators();
    assertEquals(parent, child.dominator);
    assertEquals(parent, right.dominator);
  }

  @Test
  public void subDominator() {
    // --> parent --> middle --> child
    // The child is dominated by an internal node.
    Node parent = new Node("parent");
    Node middle = new Node("middle");
    Node child = new Node("child");
    parent.depends = Arrays.asList(middle);
    middle.depends = Arrays.asList(child);

    parent.computeDominators();
    assertEquals(parent, middle.dominator);
    assertEquals(middle, child.dominator);
  }

  @Test
  public void childSelfLoop() {
    // --> parent --> child -\
    //                  \<---/
    // The child points back to itself.
    Node parent = new Node("parent");
    Node child = new Node("child");
    parent.depends = Arrays.asList(child);
    child.depends = Arrays.asList(child);

    parent.computeDominators();
    assertEquals(parent, child.dominator);
  }

  @Test
  public void singleEntryLoop() {
    // --> parent --> a --> b --> c -\
    //                 \<------------/
    // There is a loop in the graph, with only one way into the loop.
    Node parent = new Node("parent");
    Node a = new Node("a");
    Node b = new Node("b");
    Node c = new Node("c");
    parent.depends = Arrays.asList(a);
    a.depends = Arrays.asList(b);
    b.depends = Arrays.asList(c);
    c.depends = Arrays.asList(a);

    parent.computeDominators();
    assertEquals(parent, a.dominator);
    assertEquals(a, b.dominator);
    assertEquals(b, c.dominator);
  }

  @Test
  public void multiEntryLoop() {
    // --> parent --> right --> a --> b ----\
    //        \                  \<-- c <---/
    //         \--> left --->--------/
    // There is a loop in the graph, with two different ways to enter the
    // loop.
    Node parent = new Node("parent");
    Node left = new Node("left");
    Node right = new Node("right");
    Node a = new Node("a");
    Node b = new Node("b");
    Node c = new Node("c");
    parent.depends = Arrays.asList(left, right);
    right.depends = Arrays.asList(a);
    left.depends = Arrays.asList(c);
    a.depends = Arrays.asList(b);
    b.depends = Arrays.asList(c);
    c.depends = Arrays.asList(a);

    parent.computeDominators();
    assertEquals(parent, right.dominator);
    assertEquals(parent, left.dominator);
    assertEquals(parent, a.dominator);
    assertEquals(parent, c.dominator);
    assertEquals(a, b.dominator);
  }

  @Test
  public void dominatorOverwrite() {
    //            /---------> right <--\
    // --> parent  --> child <--/      /
    //            \---> left ---------/
    // Test a strange case where we have had trouble in the past with a
    // dominator getting improperly overwritten. The relevant features of this
    // case are: 'child' is visited after 'right', 'child' is dominated by
    // 'parent', and 'parent' revisits 'right' after visiting 'child'.
    Node parent = new Node("parent");
    Node right = new Node("right");
    Node left = new Node("left");
    Node child = new Node("child");
    parent.depends = Arrays.asList(left, child, right);
    left.depends = Arrays.asList(right);
    right.depends = Arrays.asList(child);

    parent.computeDominators();
    assertEquals(parent, left.dominator);
    assertEquals(parent, child.dominator);
    assertEquals(parent, right.dominator);
  }

  @Test
  public void stackOverflow() {
    // --> a --> b --> ... --> N
    // Verify we don't smash the stack for deep chains.
    Node root = new Node("root");
    Node curr = root;
    for (int i = 0; i < 10000; ++i) {
      Node node = new Node("n" + i);
      curr.depends.add(node);
      curr = node;
    }

    root.computeDominators();
  }

  @Test
  public void hiddenRevisit() {
    //           /-> left ---->---------\
    // --> parent      \---> a --> b --> c
    //           \-> right -/
    // Test a case we have had trouble in the past.
    // When a's dominator is updated from left to parent, that should trigger
    // all reachable children's dominators to be updated too. In particular,
    // c's dominator should be updated, even though b's dominator is
    // unchanged.
    Node parent = new Node("parent");
    Node right = new Node("right");
    Node left = new Node("left");
    Node a = new Node("a");
    Node b = new Node("b");
    Node c = new Node("c");
    parent.depends = Arrays.asList(right, left);
    left.depends = Arrays.asList(a, c);
    right.depends = Arrays.asList(a);
    a.depends = Arrays.asList(b);
    b.depends = Arrays.asList(c);

    parent.computeDominators();
    assertEquals(parent, left.dominator);
    assertEquals(parent, right.dominator);
    assertEquals(parent, a.dominator);
    assertEquals(parent, c.dominator);
    assertEquals(a, b.dominator);
  }

  @Test
  public void preUndominatedUpdate() {
    //       /--------->--------\
    //      /          /---->----\
    // --> p -> a --> b --> c --> d --> e
    //           \---------->----------/
    // Test a case we have had trouble in the past.
    // The candidate dominator for e is revised from d to a, then d is shown
    // to be reachable from p. Make sure that causes e's dominator to be
    // refined again from a to p. The extra nodes are there to ensure the
    // necessary scheduling to expose the bug we had.
    Node p = new Node("p");
    Node a = new Node("a");
    Node b = new Node("b");
    Node c = new Node("c");
    Node d = new Node("d");
    Node e = new Node("e");
    p.depends = Arrays.asList(d, a);
    a.depends = Arrays.asList(e, b);
    b.depends = Arrays.asList(d, c);
    c.depends = Arrays.asList(d);
    d.depends = Arrays.asList(e);

    p.computeDominators();
    assertEquals(p, a.dominator);
    assertEquals(a, b.dominator);
    assertEquals(b, c.dominator);
    assertEquals(p, d.dominator);
    assertEquals(p, e.dominator);
  }
}
