import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
const root = path.resolve(import.meta.dirname, '..');
async function get(url, target, hash) {
  try { const old = await fs.readFile(target); if (!hash || crypto.createHash('sha256').update(old).digest('hex') === hash) return; } catch {}
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${response.status}: ${url}`);
  const bytes = Buffer.from(await response.arrayBuffer());
  if (hash && crypto.createHash('sha256').update(bytes).digest('hex') !== hash) throw new Error('SHA256 mismatch: ' + url);
  await fs.mkdir(path.dirname(target), {recursive: true});
  await fs.writeFile(target, bytes);
}
const imgui = ['imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp', 'imgui.h', 'imconfig.h', 'imgui_internal.h', 'imstb_rectpack.h', 'imstb_textedit.h', 'imstb_truetype.h', 'backends/imgui_impl_win32.cpp', 'backends/imgui_impl_win32.h', 'backends/imgui_impl_dx9.cpp', 'backends/imgui_impl_dx9.h', 'LICENSE.txt'];
const minhook = ['include/MinHook.h', 'src/buffer.c', 'src/buffer.h', 'src/hook.c', 'src/trampoline.c', 'src/trampoline.h', 'src/hde/hde32.c', 'src/hde/hde32.h', 'src/hde/hde64.c', 'src/hde/hde64.h', 'src/hde/pstdint.h', 'src/hde/table32.h', 'src/hde/table64.h', 'LICENSE.txt'];
await Promise.all([
  ...imgui.map(f => get('https://raw.githubusercontent.com/ocornut/imgui/v1.91.9b/' + f, path.join(root, '.deps/imgui', f))),
  ...minhook.map(f => get('https://raw.githubusercontent.com/TsudaKageyu/minhook/v1.3.4/' + f, path.join(root, '.deps/minhook', f))),
  get('https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip', path.join(root, '.tools/zig.zip'), '3a0ed1e8799a2f8ce2a6e6290a9ff22e6906f8227865911fb7ddedc3cc14cb0c')
]);
console.log('Pinned dependencies downloaded; Zig archive SHA256 verified.');

