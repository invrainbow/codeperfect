import { A } from "@/components";
import { LINKS, SUPPORT_EMAIL } from "@/constants";

const faqs = [
  {
    q: "What makes it fast?",
    a: (
      <>
        <p>
          There isn't any one single thing. CodePerfect gets its speed largely
          by declining to copy all the extreme bloat that makes the modern tech
          stack slow. In particular, we:
        </p>
        <ol>
          <li style={{ marginBottom: "0.75em" }}>
            Use a low level language (C/C++) and render our own UI with OpenGL.
          </li>
          <li style={{ marginBottom: "0.75em" }}>
            Eschew large dependencies like Electron, language servers, etc.
          </li>
          <li style={{ marginBottom: "0.75em" }}>
            Write simple, linear-control-flow, straightforward,{" "}
            <A href={LINKS.nonPessimized}>non-pessimized</A> code, without
            unnecessary abstractions or patterns.
            <br />
          </li>
          <li>Amortize memory allocations with arenas.</li>
        </ol>
        <p>
          We especially try to limit use of third-party libraries and frameworks
          in order to own our entire stack, and maintain visibility into every
          line of code that goes into the final product. Much of the slowness of
          modern software comes not from this slow algorithm or that inefficient
          data structure, but from all the invisible bloat hidden inside the
          mainstream default software stack.
        </p>

        <p>
          We're not writing crazy inline assembly or SIMD intrinsics or
          discovering new algorithms or whatever. We do some optimization, like
          using file mappings and multithreading stuff where it makes sense, but
          mostly we are just writing straightforward code that performs the
          actual task of executing an IDE. Modern computers are just fast.
        </p>
      </>
    ),
  },
  {
    q: "What does the name mean?",
    a: (
      <>
        <p>
          It's a throwback to an era when software was way{" "}
          <A href={LINKS.oldSoftwareOpenedInstantly}>faster</A>, despite running
          on hardware much slower than a phone today.
        </p>
      </>
    ),
  },
  {
    q: "Got other questions?",
    a: (
      <p>
        Reach out at <A href={`mailto:${SUPPORT_EMAIL}`}>{SUPPORT_EMAIL}</A>.
      </p>
    ),
  },
];

export default function Faq() {
  return (
    <div className="bg-white md:bg-transparent py-12 px-6 md:px-4 md:py-24 md:max-w-screen-sm mx-auto">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title mb-8">
        FAQ
      </div>
      <div className="flex flex-col md:gap-4">
        {faqs.map((it) => (
          <div
            className="prose md:bg-white border-b border-neutral-100 last:border-0 md:border-0 md:rounded-lg md:shadow-sm group py-6 first:pt-0 last:pb-0 md:p-8 md:first:p-8 md:last:p-8"
            key={it.q}
          >
            <div className="mb-5 font-bold">{it.q}</div>
            <div className="leading-relaxed">{it.a}</div>
          </div>
        ))}
      </div>
    </div>
  );
}
