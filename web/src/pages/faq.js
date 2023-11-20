import * as faq from "@/faq";

const faqs = [
  { q: "What makes it fast?", a: faq.whatMakesFast },
  { q: "What does the name mean?", a: faq.nameMeaning },
  { q: "Got other questions?", a: faq.otherQuestions },
];

export default function Faq() {
  return (
    <div className="py-12 px-6 md:px-4 md:py-16 md:max-w-screen-sm mx-auto">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title mb-8">
        FAQ
      </div>
      <div className="flex flex-col md:gap-4">
        {faqs.map(({ q, a: Content }) => (
          <div
            className="prose border-b border-neutral-100 last:border-0 md:border-0 md:rounded-lg py-6 first:pt-0 last:pb-0 md:p-8 md:first:p-8 md:last:p-8"
            key={q}
          >
            <div className="mb-5 font-bold">{q}</div>
            <div className="leading-relaxed">
              <Content />
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
