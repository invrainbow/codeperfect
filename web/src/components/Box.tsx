import classNames from "classnames";
import { ComponentPropsWithoutRef, ElementType, ReactNode } from "react";
import { twMerge } from "tailwind-merge";

export type Props<E extends ElementType = "div"> =
  ComponentPropsWithoutRef<E> & {
    cx?: classNames.Argument;
    as?: E;
    children?: ReactNode;
  };

export function Box<E extends ElementType = "div">({
  as,
  children,
  cx,
  className,
  ...rest
}: Props<E>) {
  const As = as ?? "div";
  return (
    <As {...rest} className={twMerge(classNames(cx, className))}>
      {children}
    </As>
  );
}
