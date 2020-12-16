/*
 * Copyright (C) 2016 The Android Open Source Project
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

// Class loader that returns Helper2.class when asked to load "Test".
public class MisbehavingLoader extends DefiningLoader {
    private DefiningLoader defining_loader;

    public MisbehavingLoader(ClassLoader parent, DefiningLoader defining_loader) {
        super(parent);
        this.defining_loader = defining_loader;
    }

    protected Class<?> findClass(String name) throws ClassNotFoundException
    {
        if (name.equals("Helper1") || name.equals("Helper2")) {
            return super.findClass(name);
        } else if (name.equals("Test")) {
            throw new Error("Unexpected MisbehavingLoader.findClass(\"Test\")");
        }
        return super.findClass(name);
    }

    protected Class<?> loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        if (name.equals("Helper1") || name.equals("Helper2")) {
            return super.loadClass(name, resolve);
        } else if (name.equals("Test")) {
            // Ask for a different class.
            return defining_loader.loadClass("Helper2", resolve);
        }
        return super.loadClass(name, resolve);
    }
}
