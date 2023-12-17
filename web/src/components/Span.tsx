import { Box, Props as BoxProps } from "./Box";

export type Props = BoxProps<"span">;
export const Span = ({ ...rest }: Props) => <Box as="span" {...rest} />;
