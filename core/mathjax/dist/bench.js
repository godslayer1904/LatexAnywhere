const F = [String.raw`x^2`, String.raw`\frac{a}{b}`, String.raw`\sum_{i=1}^{N} x_i^2`, String.raw`\int_0^\infty e^{-x^2}\,dx = \frac{\sqrt{\pi}}{2}`,
	String.raw`\begin{pmatrix}1 & 2 \\ 3 & 4\end{pmatrix}`, String.raw`\left( \frac{\partial f}{\partial x} \right)^2 + \alpha\beta\gamma`,
	String.raw`\lim_{n \to \infty} \left(1+\frac{1}{n}\right)^n = e`, String.raw`\mathbb{R}^n \to \mathcal{L}(V, W)`];
const now = () => (typeof performance !== "undefined" ? performance.now() : Date.now());
let t0 = now(); LatexAnywhereMath.svg("x", false); const first = now() - t0;
const times = [];
for (let r = 0; r < 20; r++) for (const f of F) { t0 = now(); try { LatexAnywhereMath.svg(f, false); } catch (e) { if (r === 0) print("ERR", f, e.message); } times.push(now() - t0); }
times.sort((a, b) => a - b);
t0 = now(); const mml = LatexAnywhereMath.mathml(F[3], false); const mmlT = now() - t0;
print(`first ${first.toFixed(1)} ms; svg median ${times[times.length >> 1].toFixed(2)} ms, P99 ${times[Math.floor(times.length * 0.99)].toFixed(2)} ms, max ${times.at(-1).toFixed(2)} ms; mathml ${mmlT.toFixed(2)} ms`);
print(LatexAnywhereMath.svg(F[1], false).slice(0, 160));
print(mml.slice(0, 160));
