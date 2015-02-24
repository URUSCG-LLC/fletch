// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

library fletchc.expression_visitor;

import 'package:semantic_visitor/semantic_visitor.dart' show
    SemanticVisitor;

import 'package:compiler/src/constants/expressions.dart' show
    ConstantExpression;

import 'package:compiler/src/constants/values.dart' show
    ConstantValue;

import 'package:compiler/src/dart2jslib.dart' show
    MessageKind,
    Registry;

import 'package:compiler/src/elements/elements.dart';
import 'package:compiler/src/resolution/resolution.dart';
import 'package:compiler/src/tree/tree.dart';
import 'package:compiler/src/universe/universe.dart';
import 'package:compiler/src/util/util.dart' show Spannable;
import 'package:compiler/src/dart_types.dart';

import 'fletch_context.dart';

import 'fletch_function_constant.dart' show
    FletchFunctionConstant;

import '../bytecodes.dart' show
    Bytecode;

import 'fletch_selector.dart';

enum VisitState {
  Value,
  Effect,
  Test,
}

class FunctionCompiler extends SemanticVisitor {
  final FletchContext context;

  final Registry registry;

  final BytecodeBuilder builder;

  final FunctionElement function;

  final Map<ConstantValue, int> constants = <ConstantValue, int>{};

  final Map<Element, ConstantValue> functionConstantValues =
      <Element, ConstantValue>{};

  final Map<Element, int> scope = <Element, int>{};

  VisitState visitState;
  BytecodeLabel trueLabel;
  BytecodeLabel falseLabel;

  int blockLocals = 0;

  FunctionCompiler(this.context,
                   TreeElements elements,
                   this.registry,
                   FunctionElement function)
      : super(elements),
        function = function,
        builder = new BytecodeBuilder(
            function.functionSignature.parameterCount +
            (function.isInstanceMember ? 1 : 0));

  ConstantExpression compileConstant(Node node, {bool isConst}) {
    return context.compileConstant(node, elements, isConst: isConst);
  }

  void compile() {
    FunctionSignature functionSignature = function.functionSignature;
    int parameterCount = functionSignature.parameterCount;
    int i = 0;
    functionSignature.orderedForEachParameter((FormalElement parameter) {
      int slot = i++ - parameterCount - 1;
      scope[parameter] = slot;
    });

    function.node.body.accept(this);

    // Emit implicit 'return null' if no terminator is present.
    if (!builder.endsWithTerminator) {
      builder.loadLiteralNull();
      builder.ret();
    }

    builder.methodEnd();
  }

  int allocateConstant(ConstantValue constant) {
    return constants.putIfAbsent(constant, () => constants.length);
  }

  int allocateStringConstant(String string) {
    return allocateConstant(
        context.backend.constantSystem.createString(
            new DartString.literal(string)));
  }

  int allocateConstantFromFunction(FunctionElement function) {
    FletchFunctionConstant constant =
        functionConstantValues.putIfAbsent(
            function, () => new FletchFunctionConstant(function));
    return allocateConstant(constant);
  }

  int allocateConstantFromNode(Node node) {
    ConstantExpression expression = compileConstant(node, isConst: false);
    return allocateConstant(expression.value);
  }

  void invokeMethod(Selector selector) {
    registry.registerDynamicInvocation(selector);
    String symbol = context.getSymbolFromSelector(selector);
    int id = context.getSymbolId(symbol);
    int arity = selector.argumentCount;
    int fletchSelector = FletchSelector.encodeMethod(id, arity);
    builder.invokeMethod(fletchSelector, arity);
  }

  void invokeGetter(Selector selector) {
    registry.registerDynamicGetter(selector);
    String symbol = context.getSymbolFromSelector(selector);
    int id = context.getSymbolId(symbol);
    int fletchSelector = FletchSelector.encodeGetter(id);
    builder.invokeMethod(fletchSelector, 0);
  }

  // Visit the expression [node] with the result pushed on top of the stack.
  void visitForValue(Node node) {
    VisitState oldState = visitState;
    visitState = VisitState.Value;
    node.accept(this);
    visitState = oldState;
  }

  // Visit the expression [node] without the result pushed on top of the stack.
  void visitForEffect(Node node) {
    VisitState oldState = visitState;
    visitState = VisitState.Effect;
    node.accept(this);
    visitState = oldState;
  }

  // Visit the expression [node] with the result being a branch to either
  // [trueLabel] or [falseLabel].
  void visitForTest(
      Node node,
      BytecodeLabel trueLabel,
      BytecodeLabel falseLabel) {
    VisitState oldState = visitState;
    visitState = VisitState.Test;
    BytecodeLabel oldTrueLabel = this.trueLabel;
    this.trueLabel = trueLabel;
    BytecodeLabel oldFalseLabel = this.falseLabel;
    this.falseLabel = falseLabel;
    node.accept(this);
    visitState = oldState;
    this.trueLabel = oldTrueLabel;
    this.falseLabel = oldFalseLabel;
  }

  void applyVisitState() {
    if (visitState == VisitState.Effect) {
      builder.pop();
    } else if (visitState == VisitState.Test) {
      builder.branchIfTrue(trueLabel);
      builder.branch(falseLabel);
    }
  }

  // TODO(ahe): Remove this method when operators are supported in
  // AccessSemantics.
  void visitSend(Send node) {
    if (node.isOperator) {
      generateUnimplementedError(
          node, "[visitSend] for operators isn't implemented.");
    } else {
      return super.visitSend(node);
    }
  }

  void visitStaticMethodInvocation(
      Send node,
      /* MethodElement */ element,
      NodeList arguments,
      Selector selector) {
    if (element.isExternal) {
      if (element == context.compiler.backend.fletchExternalInvokeMain) {
        element = context.compiler.mainFunction;
      } else if (element == context.compiler.backend.fletchExternalYield) {
        // Handled elsewhere.
      } else {
        generateUnimplementedError(
            node, "External function ${element.name} not implemented yet.");
        return;
      }
    }
    for (Node argument in arguments) {
      visitForValue(argument);
    }
    registry.registerStaticInvocation(element);
    int methodId = allocateConstantFromFunction(element);
    builder.invokeStatic(methodId, arguments.slowLength());
    applyVisitState();
  }

  void visitDynamicInvocation(
      Send node,
      NodeList arguments,
      Selector selector) {
    visitForValue(node.receiver);
    for (Node argument in arguments) {
      visitForValue(argument);
    }
    invokeMethod(selector);
    applyVisitState();
  }

  void visitDynamicAccess(
      Send node,
      Selector selector) {
    visitForValue(node.receiver);
    invokeGetter(selector);
    applyVisitState();
  }

  void visitStaticFieldAssignment(
      SendSet node,
      FieldElement element,
      Node rhs) {
    visitForValue(rhs);
    int index = context.getStaticFieldIndex(element, function);
    builder.storeStatic(index);
    applyVisitState();
  }

  void visitStaticFieldAccess(
      Send node,
      FieldElement element) {
    Expression initializer = element.initializer;
    if (initializer != null) {
      internalError(node, "Static field initializer is not implemented");
    }
    int index = context.getStaticFieldIndex(element, function);
    builder.loadStatic(index);
    applyVisitState();
  }

  void visitLiteralNull(LiteralNull node) {
    if (visitState == VisitState.Value) {
      builder.loadLiteralNull();
    } else if (visitState == VisitState.Test) {
      builder.branch(falseLabel);
    }
  }

  void visitLiteralBool(LiteralBool node) {
    var expression = compileConstant(node, isConst: false);
    bool isTrue = expression != null && expression.value.isTrue;

    if (visitState == VisitState.Value) {
      if (isTrue) {
        builder.loadLiteralTrue();
      } else {
        builder.loadLiteralFalse();
      }
    } else if (visitState == VisitState.Test) {
      builder.branch(isTrue ? trueLabel : falseLabel);
    }
  }

  void visitLiteral(Literal node) {
    if (visitState == VisitState.Value) {
      builder.loadConst(allocateConstantFromNode(node));
    } else if (visitState == VisitState.Test) {
      builder.branch(falseLabel);
    }
  }

  void visitLiteralString(LiteralString node) {
    if (visitState == VisitState.Value) {
      builder.loadConst(allocateConstantFromNode(node));
      registry.registerInstantiatedClass(
          context.compiler.backend.stringImplementation);
    } else if (visitState == VisitState.Test) {
      builder.branch(falseLabel);
    }
  }

  void visitParenthesizedExpression(ParenthesizedExpression node) {
    // Visit expression in the same VisitState.
    node.expression.accept(this);
  }

  void visitLocalVariableAccess(
      Send node,
      LocalVariableElement element) {
    int slot = scope[element];
    builder.loadSlot(slot);
    applyVisitState();
  }

  void visitLocalVariableAssignment(
      SendSet node,
      LocalVariableElement element,
      Node rhs) {
    visitForValue(rhs);
    int slot = scope[element];
    builder.storeSlot(slot);
    applyVisitState();
  }

  void visitParameterAccess(
      Send node,
      ParameterElement element) {
    visitLocalVariableAccess(node, element);
  }

  void visitParameterAssignment(
      SendSet node,
      ParameterElement element,
      Node rhs) {
    visitLocalVariableAssignment(node, element, rhs);
  }

  void visitThrow(Throw node) {
    visitForValue(node.expression);
    builder.emitThrow();
    // TODO(ahe): It seems suboptimal that each throw is followed by a pop.
    applyVisitState();
  }

  void visitBlock(Block node) {
    int oldBlockLocals = blockLocals;
    blockLocals = 0;
    int stackSize = builder.stackSize;

    for (Node statement in node.statements) {
      statement.accept(this);
    }

    int stackSizeDifference = builder.stackSize - stackSize;
    if (stackSizeDifference != blockLocals) {
      throw "Unbalanced number of block locals and stack slots used by block.";
    }

    for (int i = 0; i < blockLocals; i++) {
      // TODO(ajohnsen): Pop range bytecode?
      builder.pop();
    }

    blockLocals = oldBlockLocals;
  }

  void visitExpressionStatement(ExpressionStatement node) {
    visitForEffect(node.expression);
  }

  void visitReturn(Return node) {
    Expression expression = node.expression;
    if (expression == null) {
      builder.loadLiteralNull();
    } else {
      visitForValue(expression);
    }
    builder.ret();
  }

  void visitStatement(Node node) {
    Visitstate oldState = visitState;
    visitState = VisitState.Effect;
    generateUnimplementedError(
        node, "Missing visit of statement: ${node.runtimeType}");
    visitState = oldState;
  }

  void visitExpression(Expression node) {
    generateUnimplementedError(
        node, "Missing visit of expression: ${node.runtimeType}");
  }

  void visitIf(If node) {
    BytecodeLabel ifTrue = new BytecodeLabel();
    BytecodeLabel ifFalse = new BytecodeLabel();
    visitForTest(node.condition, ifTrue, ifFalse);
    builder.bind(ifTrue);
    node.thenPart.accept(this);
    if (node.hasElsePart) {
      BytecodeLabel end = new BytecodeLabel();
      builder.branch(end);
      builder.bind(ifFalse);
      node.elsePart.accept(this);
      builder.bind(end);
    } else {
      builder.bind(ifFalse);
    }
  }

  void visitWhile(While node) {
    BytecodeLabel start = new BytecodeLabel();
    BytecodeLabel ifTrue = new BytecodeLabel();
    BytecodeLabel end = new BytecodeLabel();
    builder.bind(start);
    visitForTest(node.condition, ifTrue, end);
    builder.bind(ifTrue);
    node.body.accept(this);
    builder.branch(start);
    builder.bind(end);
  }

  void visitDoWhile(DoWhile node) {
    BytecodeLabel start = new BytecodeLabel();
    BytecodeLabel end = new BytecodeLabel();
    builder.bind(start);
    node.body.accept(this);
    visitForTest(node.condition, start, end);
    builder.bind(end);
  }

  void visitVariableDefinitions(VariableDefinitions node) {
    for (Node definition in node.definitions) {
      int slot = builder.stackSize;
      LocalVariableElement element = elements[definition];
      Expression initializer = element.initializer;
      if (initializer == null) {
        builder.loadLiteralNull();
      } else {
        visitForValue(initializer);
      }
      scope[element] = slot;
      blockLocals++;
    }
  }

  void visitFunctionExpression(FunctionExpression node) {
    generateUnimplementedError(
        node, "[visitFunctionExpression] isn't implemented.");
  }

  void visitParameterInvocation(
      Send node,
      ParameterElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitParameterInvocation] isn't implemented.");
  }

  void visitLocalVariableInvocation(
      Send node,
      LocalVariableElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitLocalVariableInvocation] isn't implemented.");
  }

  void visitLocalFunctionAccess(
      Send node,
      LocalFunctionElement element) {
    generateUnimplementedError(
        node, "[visitLocalFunctionAccess] isn't implemented.");
  }

  void visitLocalFunctionAssignment(
      SendSet node,
      LocalFunctionElement element,
      Node rhs,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitLocalFunctionAssignment] isn't implemented.");
  }

  void visitLocalFunctionInvocation(
      Send node,
      LocalFunctionElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitLocalFunctionInvocation] isn't implemented.");
  }

  void visitDynamicAssignment(
      SendSet node,
      Selector selector,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitDynamicAssignment] isn't implemented.");
  }

  void visitStaticFieldInvocation(
      Send node,
      FieldElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitStaticFieldInvocation] isn't implemented.");
  }

  void visitStaticMethodAccess(
      Send node,
      MethodElement element) {
    generateUnimplementedError(
        node, "[visitStaticMethodAccess] isn't implemented.");
  }

  void visitStaticPropertyAccess(
      Send node,
      FunctionElement element) {
    generateUnimplementedError(
        node, "[visitStaticPropertyAccess] isn't implemented.");
  }

  void visitStaticPropertyAssignment(
      SendSet node,
      FunctionElement element,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitStaticPropertyAssignment] isn't implemented.");
  }

  void visitStaticPropertyInvocation(
      Send node,
      FieldElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitStaticPropertyInvocation] isn't implemented.");
  }

  void visitTopLevelFieldAccess(
      Send node,
      FieldElement element) {
    generateUnimplementedError(
        node, "[visitTopLevelFieldAccess] isn't implemented.");
  }

  void visitTopLevelFieldAssignment(
      SendSet node,
      FieldElement element,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitTopLevelFieldAssignment] isn't implemented.");
  }

  void visitTopLevelFieldInvocation(
      Send node,
      FieldElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitTopLevelFieldInvocation] isn't implemented.");
  }

  void visitTopLevelMethodAccess(
      Send node,
      MethodElement element) {
    generateUnimplementedError(
        node, "[visitTopLevelMethodAccess] isn't implemented.");
  }

  void visitTopLevelMethodInvocation(
      Send node,
      MethodElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitTopLevelMethodInvocation] isn't implemented.");
  }

  void visitTopLevelPropertyAccess(
      Send node,
      FunctionElement element) {
    generateUnimplementedError(
        node, "[visitTopLevelPropertyAccess] isn't implemented.");
  }

  void visitTopLevelPropertyAssignment(
      SendSet node,
      FunctionElement element,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitTopLevelPropertyAssignment] isn't implemented.");
  }

  void visitTopLevelPropertyInvocation(
      Send node,
      FieldElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitTopLevelPropertyInvocation] isn't implemented.");
  }

  void visitClassTypeLiteralAccess(
      Send node,
      ClassElement element) {
    generateUnimplementedError(
        node, "[visitClassTypeLiteralAccess] isn't implemented.");
  }

  void visitClassTypeLiteralInvocation(
      Send node,
      ClassElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitClassTypeLiteralInvocation] isn't implemented.");
  }

  void visitClassTypeLiteralAssignment(
      SendSet node,
      ClassElement element,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitClassTypeLiteralAssignment] isn't implemented.");
  }

  void visitTypedefTypeLiteralAccess(
      Send node,
      TypedefElement element) {
    generateUnimplementedError(
        node, "[visitTypedefTypeLiteralAccess] isn't implemented.");
  }

  void visitTypedefTypeLiteralInvocation(
      Send node,
      TypedefElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitTypedefTypeLiteralInvocation] isn't implemented.");
  }

  void visitTypedefTypeLiteralAssignment(
      SendSet node,
      TypedefElement element,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitTypedefTypeLiteralAssignment] isn't implemented.");
  }

  void visitTypeVariableTypeLiteralAccess(
      Send node,
      TypeVariableElement element) {
    generateUnimplementedError(
        node, "[visitTypeVariableTypeLiteralAccess] isn't implemented.");
  }

  void visitTypeVariableTypeLiteralInvocation(
      Send node,
      TypeVariableElement element,
      NodeList arguments,
      Selector selector) {
    generateUnimplementedError(
        node, "[visitTypeVariableTypeLiteralInvocation] isn't implemented.");
  }

  void visitTypeVariableTypeLiteralAssignment(
      SendSet node,
      TypeVariableElement element,
      Node rhs) {
    generateUnimplementedError(
        node, "[visitTypeVariableTypeLiteralAssignment] isn't implemented.");
  }

  void visitDynamicTypeLiteralAccess(
      Send node) {
    generateUnimplementedError(
        node, "[visitDynamicTypeLiteralAccess] isn't implemented.");
  }

  void visitAssert(
      Send node,
      Node expression) {
    generateUnimplementedError(
        node, "[visitAssert] isn't implemented.");
  }

  void internalError(Spannable spannable, String reason) {
    context.compiler.internalError(spannable, reason);
  }

  void generateUnimplementedError(Spannable spannable, String reason) {
    context.compiler.reportError(
        spannable, MessageKind.GENERIC, {'text': reason});
    builder.loadConst(allocateStringConstant(reason));
    builder.emitThrow();
    applyVisitState();
  }

  String toString() => "FunctionCompiler(${function.name})";

  String verboseToString() {
    StringBuffer sb = new StringBuffer();

    sb.writeln("Constants:");
    constants.forEach((constant, int index) {
      if (constant is ConstantValue) {
        constant = constant.toStructuredString();
      }
      sb.writeln("  #$index: $constant");
    });

    sb.writeln("Bytecodes:");
    int offset = 0;
    for (Bytecode bytecode in builder.bytecodes) {
      sb.writeln("  $offset: $bytecode");
      offset += bytecode.size;
    }

    return '$sb';
  }
}