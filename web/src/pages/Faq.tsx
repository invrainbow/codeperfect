import { marked } from "marked";

import nameMeaningMd from "@/faq/name-meaning.md.txt?raw";
import otherQuestionsMd from "@/faq/other-questions.md.txt?raw";
import whatMakesFastMd from "@/faq/what-makes-fast.md.txt?raw";

const QUESTIONS = [
  marked(whatMakesFastMd),
  marked(nameMeaningMd),
  marked(otherQuestionsMd),
];

export const Faq = () => (
  <div className="py-12 px-6 md:px-4 md:py-16 md:max-w-screen-sm mx-auto">
    <div className="md:px-4 md:text-center text-3xl md:text-5xl title mb-8">
      FAQ
    </div>
    <div className="flex flex-col md:gap-4">
      {QUESTIONS.map((it, idx) => (
        <div
          className="faq border-b border-neutral-100 last:border-0 md:border-0 md:rounded-lg py-6 first:pt-0 last:pb-0 md:p-4 md:first:p-4 md:last:p-4 leading-normal"
          dangerouslySetInnerHTML={{ __html: it }}
          key={idx}
        />
      ))}
    </div>
  </div>
);
