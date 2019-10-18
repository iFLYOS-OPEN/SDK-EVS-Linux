/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import "tests/jerry/es2015/module-export-01.js";
import def from "tests/jerry/es2015/module-export-01.js";
import {} from "tests/jerry/es2015/module-export-01.js";
import {aa as a,} from "tests/jerry/es2015/module-export-01.js";
import {bb as b, cc as c} from "tests/jerry/es2015/module-export-01.js";
import {x} from "tests/jerry/es2015/module-export-01.js";
import * as mod from "tests/jerry/es2015/module-export-01.js";

assert (def === "default");
assert (a === "a");
assert (b === 5);
assert (c(b) === 10);
assert (Array.isArray(mod.d))
assert (x === 42)
assert (mod.f("str") === "str")

dog = new mod.Dog("Oddie")
assert (dog.speak() === "Oddie barks.")
