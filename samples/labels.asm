		;
		; test labels
		;

		extern extern_label

		public public_label
		public public_equ, public_gequ

private_label
public_label

public_equ			equ $1234
public_gequ			gequ $1234

private_equ			equ $1234
private_gequ		gequ $1234

			globals		on

			page0
			nop
page0_equ	equ $1234
page0_label
			ends


			code
			nop
code_equ	equ $1234
code_label
			ends

			data
			nop
data_equ	equ $1234
data_label
			ends

			udata
			nop
udata_equ	equ $1234
udata_label
			ends

			kdata
			nop
kdata_equ	equ $1234
kdata_label
			ends

offset_s	section	offset $200
			nop
offset_equ	equ *
offset_label
			ends

indir_s	section	indirect $200
			nop
indir_equ	equ *
indir_label
			ends

