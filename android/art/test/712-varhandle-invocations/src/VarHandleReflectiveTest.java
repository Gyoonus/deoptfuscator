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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class VarHandleReflectiveTest {
    public static class ReflectiveAccessorInvocations extends VarHandleUnitTest {
        private static final VarHandle vh;
        private static int field;

        static {
            try {
                Class<?> cls = ReflectiveAccessorInvocations.class;
                vh = MethodHandles.lookup().findStaticVarHandle(cls, "field", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        protected void doTest() throws Exception {
            for (VarHandle.AccessMode accessMode : VarHandle.AccessMode.values()) {
                Method accessorMethod =
                        VarHandle.class.getMethod(accessMode.methodName(), Object[].class);
                try {
                    accessorMethod.invoke(vh, new Object[] {new Object[] {}});
                    failUnreachable();
                } catch (InvocationTargetException e) {
                    assertEquals(e.getCause().getClass(), UnsupportedOperationException.class);
                }
            }
        }

        public static void main(String[] args) {
            new ReflectiveAccessorInvocations().run();
        }
    }

    public static void main(String[] args) {
        ReflectiveAccessorInvocations.main(args);
    }
}
