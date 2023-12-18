import classNames from "classnames";
import { ComponentProps, h, JSX } from "preact";
import { twMerge } from "tailwind-merge";

export type WrappedProps<K extends JSX.ElementType> = ComponentProps<K> & {
  cx?: classNames.Argument;
};

export const wrapElement = <K extends JSX.ElementType>(
  C: K,
  { cx, ...props }: WrappedProps<K>
) => h(C as any, { className: twMerge(classNames(cx)), ...props });

export const wrap = <K extends JSX.ElementType>(C: K) => {
  return (props: WrappedProps<K>) => wrapElement(C, props);
};

// ...

export type DivProps = WrappedProps<"div">;
export type ImageProps = WrappedProps<"img">;
export type AProps = WrappedProps<"a">;
export type SpanProps = WrappedProps<"span">;

export default {
  div: wrap("div"),
  img: wrap("img"),
  a: wrap("a"),
  span: wrap("span"),
};
