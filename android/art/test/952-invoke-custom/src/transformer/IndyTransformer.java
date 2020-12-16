/*
 * Copyright (C) 2018 The Android Open Source Project
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
package transformer;

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import annotations.Constant;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;

/**
 * Class for inserting invoke-dynamic instructions in annotated Java class files.
 *
 * <p>This class replaces static method invocations of annotated methods with invoke-dynamic
 * instructions. Suppose a method is annotated as: <code>
 *
 * @CalledByIndy(
 *      bootstrapMethod =
 *          @BootstrapMethod(
 *               enclosingType = TestLinkerMethodMinimalArguments.class,
 *               parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class},
 *               name = "linkerMethod"
 *     ),
 *     fieldOdMethodName = "magicAdd",
 *     returnType = int.class,
 *     argumentTypes = {int.class, int.class}
 * )
 * private int add(int x, int y) {
 *    throw new UnsupportedOperationException(e);
 * }
 *
 * private int magicAdd(int x, int y) {
 *    return x + y;
 * }
 *
 * </code> Then invokestatic bytecodes targeting the add() method will be replaced invokedynamic
 * instructions targetting the CallSite that is construction by the bootstrap method described by
 * the @CalledByIndy annotation.
 *
 * <p>In the example above, this results in add() being replaced by invocations of magicAdd().
 */
class IndyTransformer {

    static class BootstrapBuilder extends ClassVisitor {

        private final Map<String, CalledByIndy> callsiteMap;
        private final Map<String, Handle> bsmMap = new HashMap<>();

        public BootstrapBuilder(int api, Map<String, CalledByIndy> callsiteMap) {
            this(api, null, callsiteMap);
        }

        public BootstrapBuilder(int api, ClassVisitor cv, Map<String, CalledByIndy> callsiteMap) {
            super(api, cv);
            this.callsiteMap = callsiteMap;
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String desc, String signature, String[] exceptions) {
            MethodVisitor mv = cv.visitMethod(access, name, desc, signature, exceptions);
            return new MethodVisitor(this.api, mv) {
                @Override
                public void visitMethodInsn(
                        int opcode, String owner, String name, String desc, boolean itf) {
                    if (opcode == org.objectweb.asm.Opcodes.INVOKESTATIC) {
                        CalledByIndy callsite = callsiteMap.get(name);
                        if (callsite != null) {
                            insertIndy(callsite.fieldOrMethodName(), desc, callsite);
                            return;
                        }
                    }
                    mv.visitMethodInsn(opcode, owner, name, desc, itf);
                }

                private void insertIndy(String name, String desc, CalledByIndy callsite) {
                    Handle bsm = buildBootstrapMethodHandle(callsite.bootstrapMethod()[0]);
                    Object[] bsmArgs =
                            buildBootstrapArguments(callsite.constantArgumentsForBootstrapMethod());
                    mv.visitInvokeDynamicInsn(name, desc, bsm, bsmArgs);
                }

                private Handle buildBootstrapMethodHandle(BootstrapMethod bootstrapMethod) {
                    String className = Type.getInternalName(bootstrapMethod.enclosingType());
                    String methodName = bootstrapMethod.name();
                    String methodType =
                            MethodType.methodType(
                                            bootstrapMethod.returnType(),
                                            bootstrapMethod.parameterTypes())
                                    .toMethodDescriptorString();
                    return new Handle(
                            Opcodes.H_INVOKESTATIC,
                            className,
                            methodName,
                            methodType,
                            false /* itf */);
                }

                private Object decodeConstant(int index, Constant constant) {
                    if (constant.booleanValue().length == 1) {
                        return constant.booleanValue()[0];
                    } else if (constant.byteValue().length == 1) {
                        return constant.byteValue()[0];
                    } else if (constant.charValue().length == 1) {
                        return constant.charValue()[0];
                    } else if (constant.shortValue().length == 1) {
                        return constant.shortValue()[0];
                    } else if (constant.intValue().length == 1) {
                        return constant.intValue()[0];
                    } else if (constant.longValue().length == 1) {
                        return constant.longValue()[0];
                    } else if (constant.floatValue().length == 1) {
                        return constant.floatValue()[0];
                    } else if (constant.doubleValue().length == 1) {
                        return constant.doubleValue()[0];
                    } else if (constant.stringValue().length == 1) {
                        return constant.stringValue()[0];
                    } else if (constant.classValue().length == 1) {
                        return Type.getType(constant.classValue()[0]);
                    } else {
                        throw new Error("Bad constant at index " + index);
                    }
                }

                private Object[] buildBootstrapArguments(Constant[] bootstrapMethodArguments) {
                    Object[] args = new Object[bootstrapMethodArguments.length];
                    for (int i = 0; i < bootstrapMethodArguments.length; ++i) {
                        args[i] = decodeConstant(i, bootstrapMethodArguments[i]);
                    }
                    return args;
                }
            };
        }
    }

    private static void transform(Path inputClassPath, Path outputClassPath) throws Throwable {
        URLClassLoader classLoader =
                new URLClassLoader(
                        new URL[] {inputClassPath.toUri().toURL()},
                        ClassLoader.getSystemClassLoader());
        String inputClassName = inputClassPath.getFileName().toString().replace(".class", "");
        Class<?> inputClass = classLoader.loadClass(inputClassName);
        Map<String, CalledByIndy> callsiteMap = new HashMap<>();

        for (Method m : inputClass.getDeclaredMethods()) {
            CalledByIndy calledByIndy = m.getAnnotation(CalledByIndy.class);
            if (calledByIndy == null) {
                continue;
            }
            if (calledByIndy.fieldOrMethodName() == null) {
                throw new Error("CallByIndy annotation does not specify a field or method name");
            }
            final int PRIVATE_STATIC = Modifier.STATIC | Modifier.PRIVATE;
            if ((m.getModifiers() & PRIVATE_STATIC) != PRIVATE_STATIC) {
                throw new Error(
                        "Method whose invocations should be replaced should be private and static");
            }
            callsiteMap.put(m.getName(), calledByIndy);
        }
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
        try (InputStream is = Files.newInputStream(inputClassPath)) {
            ClassReader cr = new ClassReader(is);
            cr.accept(new BootstrapBuilder(Opcodes.ASM6, cw, callsiteMap), 0);
        }
        try (OutputStream os = Files.newOutputStream(outputClassPath)) {
            os.write(cw.toByteArray());
        }
    }

    public static void main(String[] args) throws Throwable {
        transform(Paths.get(args[0]), Paths.get(args[1]));
    }
}
